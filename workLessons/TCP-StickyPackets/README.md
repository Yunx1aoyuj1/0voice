    最近接手了一个离职同事没做完的外部接口（大坑，能编译通过就让他走了，走之前也没说让我接手，没有做任何交接），QDL6项目的外部接口，基于TCP的网络传输协议，但是他毕业之后一直在做UDP协议相关，刚刚开始的TCP数据粘包就没处理好。

    TCP和UDP收发数据的差异：

    1、TCP协议层会做分包处理
    2、UDP直接send多少recv多少，协议层面不做分包处理
    3、IO读写事件触发之后，UDP程序获取到的缓冲区数据可以直接解析headdata通过MessageType进行数据解析。而TCP由于存在分包的可能，从缓冲区一次获取的数据可能解析不出来一个完整的MessageData.
    4、所以TCP传输协议需要一直读缓冲区，按顺序排列组成数据池，根据headdata数据解析出每一包的数据长度，然后从数据池里拷贝数据组完整的包.


    验证完放一点代码段进行说明吧，留个纪念~
    参考链接：
    https://www.zhihu.com/column/easyserverdev


    大概的接口内容：
    1、dataHead：
        SystemId、TotalLength、Multi-flag、PackageData
        其中：
            a、每包最大长度1025、Multi-flag表示是否分包
            b、PackageData包含多个 messageData
            c、每个 messageData 包含：msgLength、msgId、msgData。

    解包时候需要考虑的问题：
    1、找到dataHead，解析出length、multiFlag；
    2、当multiFlag = 0时，两种情况：
        a、上一包multiFlag = 0，当前这一包数据一定是从dataHead开始；
        b、上一包multiFlag = 1，需要考虑上一包数据中msgData部分没发完的情况；
    3、当multiFlag = 1时，两种情况：
        a、上一包multiFlag = 0，当前这一包数据一定是从dataHead开始；
        b、上一包multiFlag = 1，需要考虑上一包数据中msgData部分没发完的情况；

    数据结构设计：
    1、messageData：
        msgId，msgLength，msgBuff；
    2、messagePool：
        a、一个较大的数组，length长度固定，writePos写入位置，readPos读取位置；
        b、每次writePos、readPos往后挪一位之后，对length取模，且readPos < writePos;
        c、存储去掉dataHead的数据，只保留每个msgData结构的长度和buffer部分。
        d、因为可能一包数据组不了一个messageData，需要将多个数据包组合。
    3、解析出dataHead后跟收到的len比较，计算是否len不够，下一包需要先解析多少数据再去除dataHead。

    伪代码：

    int pos = 0;
    static int preRemLength = 0;
    while(pos < len)
    {
        if(preRemLength == 0)
        {
            if(buf[0] != 0xff)
            {
                errlog<< "systemId is err."<<std::endl;
                return false;
            }

            int totalLength = (buf[1] | (buf[2] << 8));
            pos += 4;
            preRemLength += totalLength + pos;
        }

        int writeLen = 0;
        if(preRemLength < len)
        {
            writeLen = preRemLength;
            preRemLength = 0;
        }
        else
        {
            writeLen = len;
            preRemLength = preRemLength % len;
        }

        for(pos; pos < writeLen; ++pos)
        {
            m_messageBuf[m_writePos++] = buf[pos];
            m_writePos = m_writePos % m_maxMessageSize;
        }
    }

    bRet = resolveMessage();    //resolveMessage() 先解析前两个字节，判断当前存储的长度是否够一个msgData，不够直接返回，够的话循环解析msgData然后生产MessageDataPtr对象存储.


