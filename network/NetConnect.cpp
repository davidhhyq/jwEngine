#include "NetConnect.h"
#include "TcpPacketFactory.h"
#include "NetEvent.h"

NetConnect::NetConnect(NetEvent * nevent,uint32 buffersize) :
	_netevent(nevent),
	TcpSocket(buffersize),
	mReadPacket(NULL)
{
}

NetConnect::~NetConnect()
{
	if (mReadPacket)
	{
		recyclePacket(mReadPacket);
		mReadPacket = NULL;
	}
}

void NetConnect::on_msgbuffer(MessageBuffer * buffer)
{
	if (!mReadPacket)
	{
		//create packet obj
		mReadPacket = createPacket();
	}

	while (buffer->GetActiveSize() > 0)
	{
		//read head
		if (mReadPacket->rpos() < MSG_HEAD_SIZE)
		{
			uint32 rlen = mReadPacket->readHead(buffer->GetReadPointer(), buffer->GetActiveSize());
			buffer->ReadCompleted(rlen);
		}
		else
		{
			//read body
			int32 needsize = mReadPacket->getMsgLen() + MSG_HEAD_SIZE - mReadPacket->wpos();
			if (needsize > 0)
			{
				int32 wsize = buffer->GetActiveSize() >= needsize ? needsize : buffer->GetActiveSize();
				if (wsize > 0)
				{
					mReadPacket->append(buffer->GetReadPointer(), wsize);
					buffer->ReadCompleted(wsize);
				}
			}

			//new packet
			if (mReadPacket->wpos() == mReadPacket->getMsgLen() + MSG_HEAD_SIZE)
			{
				_netevent->onMsg(this, mReadPacket);

				// recycle packet
				recyclePacket(mReadPacket);
				mReadPacket = NULL;
			}
		}
	}//while
}
void NetConnect::on_clsesocket()
{
	_netevent->onClose(this);
	_netevent->destroyConnect(this);
}


void NetConnect::sendMsg(uint32 msgtype, NetPacket * pack){

	pack->setMsgLen(pack->wpos() - MSG_HEAD_SIZE);
	pack->setMsgType(msgtype);
	pack->writeHead();

	mSendPackets.push(pack);

	send_top_msg();
}

void NetConnect::sendMsg(uint32 msgtype, void * msg, uint32 len)
{

	NetPacket *pack = createPacket();

	pack->append((uint8 *)msg, len);
	pack->setMsgLen(len);
	pack->setMsgType(msgtype);
	pack->writeHead();

	mSendPackets.push(pack);

	send_top_msg();
}


NetPacket * NetConnect::createPacket()
{
	NetPacket * packet = CREATE_TCP_PACKET;
	packet->zero();
	return packet;
}

void NetConnect::recyclePacket(NetPacket * pack)
{
	if (pack)
		RECYCLE_TCP_PACKET(pack);
}

void NetConnect::on_writecomplete()
{
	//write complete
	recyclePacket(mSendPackets.back());

	mSendPackets.pop();
	send_top_msg();
}

void NetConnect::send_top_msg()
{
	if (mSendPackets.size() == 0)
		return;

	NetPacket *tp = mSendPackets.back();
	write((char *)tp->getData(), tp->wpos());
}