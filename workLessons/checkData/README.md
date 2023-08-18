最近遇到的一个坑：
场景：
    1、先跑出来TLS定位和ATP定位；
    2、关闭GNSS设备，使用ATP定位往前跑，综合监控上列车图标以及位置正常；
    3、关闭列车ATP定位，综合监控列车图标跳变到关闭GNSS设备的位置。

经过分析（扯皮），最终定位到，TLS软件接收OBS发送的GPS数据对非法值的处理（PS：使用这种方式已经跑了很多很多年了~）

详细记录一下这个项目的定位方式切换，以及遇到的GPS数据非法值导致列车位置跳变。

缩写：
1、TLS          综合监控的位置翻译服务器，接收OBS发送的列车位置数据，解析并转换给SRS能识别的列车位置；
2、OBS          车载软件，会给TLS发送车头车尾GPS数据和SDU数据；会给SRS发送车头车尾Edge和offset；
3、SRS          综合监控的服务器
4、GPS          列车上接收的GPS数据
5、SDU          通过轨道上安装的定位设备，扫描到vetra设备后发送相对于这个设备点的距离、速度、方向等信息


首先，XX项目前景提要：
1、有轨项目以前的定位方式为TLS定位（OBS发送GPS和信标数据给TLS，TLS翻译出Edge和offset数据发给SRS）
2、新增OBS直接发给SRS定位数据（Edge和offset），称为OBS定位
3、两种定位方式都要，优先使用OBS，且为了配合测试，新增接口给用户添加可以选择只使用某一种定位方式。

然后，我给的定位模式切换算法：
1、在ITrain对象中将列车定位相关数据新增OBS模式和TLS模式，例如：direction、obs_direction、tls_direction 列车方向相关的数据。
2、新增obs_direction、tls_direction对应事件的订阅，触发到obs_direction、tls_direction事件时，检查OBS和TLS的定位状态（是否手动设置了定位模式）

    case ITrain::EV_OBSDIRECTION:
        {
            //  XXX ==> OBS / TLS
            if(isUseXXXDataInfo)
            {
                setDirection(getXXXdirection());
            }
        }
        break;

    bool CTrain::isUseOBSDataInfo()
    {
        bool enable = false;
        if (((getTramLocationMode() == ITrain::INTELLIGENT_LOCATION_MODE) 
                && (getObsLocationStatus() != ITrain::NOT_LOCATION)) //自动定位模式并且OBS定位状态不为未定位
            || (getTramLocationMode() == ITrain::OBS_LOCATION_MODE
                && getCommunicationStatus() == ITrain::COMMUNICATING))		//手动设置OBS定位模式 && 通信车,非通信车不做处理
        {
            enable = true;
        }
        return enable;
    }
    //当定位状态发生变化的时候也会触发订阅事件，从而切换定位数据。

3、现在ITrain对象中每种数据有三组，例如direction数据，direction为当前综合监控使用的列车方向数据，tls_direction为TLS发送的列车方向数据，obs_direction为OBS发送的列车方向数据，三组数据都进行记录的意义在于保存回放数据，当需要查看回放的时候可以切换使用不同的数据检查问题。

分析问题：
1、PR中写了先关闭GNSS（TLS定位状态为未定位，或者使用SDU定位数据）跑一段距离后关闭ATP定位（OBS定位状态为未定位），综合监控列车图标跳回关GNSS设备的位置。
2、先看抓包确认对方发包问题还是SRS的问题，抓包数据显示OBS发的列车定位数据会解析出未定位状态（正常），OBS发给TLS的定位数据中（SDU数据正常：信标ID、距离、速度都对；GPS数据为全0，看着也没啥问题），TLS和SRS为内部软件没写解包脚本（领导说是永远都不会变通信接口，不需要写。。。）只能看日志，TLS一直给SRS发送定位状态为精确定位，定位数据和GPS数值正确，但是数据包在列车移动期间不变，判断TLS解析数据出错或者拒包，但维持了原来的数据没更新一直发送。
3、查看TLS日志发现关闭GNSS设备期间（GPS数据为0的期间），一直重复打印一行日志，查看代码发现是程序抛出异常。
4、检查发现：OBS给TLS发送GPS数据使用的格式为ASCII码，TLS使用boost库中的lexical_cast将cstring类型数据转换为double类型数据，但是车载关闭GNSS设备时，OBS接收不到GPS数据，直接按照16进制填充了全0，是一个非法的GPS数据格式，导致boost::lexical_cast抛出异常，但是TLS历史遗留问题只接收了异常打印了日志然后结束。

好的，剩下的是领导跟同事battle时间：
1、为啥测试这么多年了没发现，是不是OBS改了GPS非法数据格式；
2、虽然综合监控没有检查GPS数据的合法格式，但是OBS应该按照GPS数据格式发送.
3、巴拉巴拉。。。

解决方案：
有两种解决办法：
1、OBS按照GPS数据格式发送默认GPS数据；
2、使用boost::lexical_cast进行数据格式转换之前，检查cstring类型数据中，是否都是整数，使用boost::algorithm::all：

    bool isInteger = boost::algorithm::all(boost::trim_copy(stringStreamLon.str()), boost::algorithm::is_digit());



完事~
去写下一个BUG。


补充：
1、手动解析了一下OBS发给TLS数据包中的GPS数据，发现有小数点，所以不能用上述第二个解决方案，有个更简洁的：

    //直接使用抛异常
    ods_double64 lat = 0;
    
    try 
    {
        lat = boost::lexical_cast<ods_double64>(stringStreamLat.str());
    }
    catch (const boost::bad_lexical_cast& e) 
    {
        flagLa = 'N';
    }

结束.