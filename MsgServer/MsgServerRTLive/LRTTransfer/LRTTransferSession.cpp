//
//  LRTTransferSession.cpp
//  dyncRTRTLive
//
//  Created by hp on 11/26/15.
//  Copyright (c) 2015 hp. All rights reserved.
//

#include <list>
#include "LRTTransferSession.h"
#include "RTUtils.hpp"
#include "LRTConnManager.h"
#include "LRTRTLiveManager.h"

#include "MsgServer/proto/common_msg.pb.h"
#include "MsgServer/proto/sys_msg_type.pb.h"
#include "MsgServer/proto/storage_msg_type.pb.h"
#include "MsgServer/proto/sys_msg.pb.h"
#include "MsgServer/proto/storage_msg.pb.h"
#include "MsgServer/proto/entity_msg.pb.h"
#include "MsgServer/proto/entity_msg_type.pb.h"

#define TIMEOUT_TS (60*1000)
#define MSG_PACKED_ONCE_NUM (10)

static int g_on_ticket_time = 0;

LRTTransferSession::LRTTransferSession()
: RTJSBuffer()
, RTTransfer()
, m_lastUpdateTime(0)
, m_transferSessId("")
, m_moduleId("")
, m_addr("")
, m_port(0)
, m_connectingStatus(0)
, m_wNewMsgId(0)
{
    AddObserver(this);
}

LRTTransferSession::~LRTTransferSession()
{
    DelObserver(this);
    Unit();
}

void LRTTransferSession::Init()
{
    TCPSocket* socket = this->GetSocket();

    socket->Open();

    socket->InitNonBlocking(socket->GetSocketFD());
    socket->NoDelay();
    socket->KeepAlive();
    socket->SetSocketBufSize(96L * 1024L);

    socket->SetTask(this);
    this->SetTimer(120*1000);
    for(int i=0;i<MSG_PACKED_ONCE_NUM;++i)
    {
         m_packedNewMsg.add_msgs();
    }
    for(int i=0;i<MSG_PACKED_ONCE_NUM;++i)
    {
         m_packedSeqnMsg.add_msgs();
    }
    for(int i=0;i<MSG_PACKED_ONCE_NUM;++i)
    {
         m_packedDataMsg.add_msgs();
    }

}

void LRTTransferSession::InitConf()
{
    for(int i=0;i<MSG_PACKED_ONCE_NUM;++i)
    {
         m_packedNewMsg.add_msgs();
    }
    for(int i=0;i<MSG_PACKED_ONCE_NUM;++i)
    {
         m_packedSeqnMsg.add_msgs();
    }
    for(int i=0;i<MSG_PACKED_ONCE_NUM;++i)
    {
         m_packedDataMsg.add_msgs();
    }
}

void LRTTransferSession::Unit()
{
    Disconn();
    m_connectingStatus = 0;
}

bool LRTTransferSession::Connect(const std::string addr, int port)
{
    m_addr = addr;
    m_port = port;
    OS_Error err = GetSocket()->Connect(SocketUtils::ConvertStringToAddr(m_addr.c_str()), m_port);
    if (err == OS_NoErr || err == EISCONN) {
        m_connectingStatus = 1;
        return true;
    } else {
        LE("%s ERR:%d\n", __FUNCTION__, err);
        return false;
    }
}

bool LRTTransferSession::Connect()
{
    if (m_addr.empty() || m_port < 2048) {
        LE("%s invalid params addr:%s, port:%d\n", __FUNCTION__, m_addr.c_str(), m_port);
        return false;
    }
    OS_Error err = GetSocket()->Connect(SocketUtils::ConvertStringToAddr(m_addr.c_str()), m_port);
    if (err == OS_NoErr || err == EISCONN) {
        m_connectingStatus = 1;
        return true;
    } else {
        LE("%s ERR:%d\n", __FUNCTION__, err);
        return false;
    }
}

void LRTTransferSession::Disconn()
{
    GetSocket()->Cleanup();
}

bool LRTTransferSession::RefreshTime()
{
    UInt64 now = OS::Milliseconds();
    if (m_lastUpdateTime <= now) {
        m_lastUpdateTime = now  + TIMEOUT_TS;
        RTTcpNoTimeout::UpdateTimer();
        return true;
    } else {
        return false;
    }
}

void LRTTransferSession::KeepAlive()
{
#if DEF_PROTO
    pms::TransferMsg t_msg;
    pms::ConnMsg c_msg;

    c_msg.set_tr_module(pms::ETransferModule::MLIVE);
    c_msg.set_conn_tag(pms::EConnTag::TKEEPALIVE);

    t_msg.set_type(pms::ETransferType::TCONN);
    t_msg.set_flag(pms::ETransferFlag::FNOACK);
    t_msg.set_priority(pms::ETransferPriority::PNORMAL);
    t_msg.set_content(c_msg.SerializeAsString());

    std::string s = t_msg.SerializeAsString();
    SendTransferData(s.c_str(), (int)s.length());
#else
    LE("not define DEF_PROTO\n");
#endif
}

void LRTTransferSession::TestConnection()
{

}

void LRTTransferSession::EstablishConnection()
{
#if DEF_PROTO
    pms::TransferMsg t_msg;
    pms::ConnMsg c_msg;

    c_msg.set_tr_module(pms::ETransferModule::MLIVE);
    c_msg.set_conn_tag(pms::EConnTag::THI);

    t_msg.set_type(pms::ETransferType::TCONN);
    t_msg.set_flag(pms::ETransferFlag::FNEEDACK);
    t_msg.set_priority(pms::ETransferPriority::PHIGH);
    t_msg.set_content(c_msg.SerializeAsString());

    std::string s = t_msg.SerializeAsString();
    SendTransferData(s.c_str(), (int)s.length());
#else
    LE("not define DEF_PROTO\n");
#endif
}

void LRTTransferSession::SendTransferData(const char* pData, int nLen)
{
    LRTRTLiveManager::Instance().SendResponseCounter();
    RTTcpNoTimeout::SendTransferData(pData, nLen);
    GetSocket()->RequestEvent(EV_RE);
}

void LRTTransferSession::SendTransferData(const std::string& data)
{
    SendTransferData(data.c_str(), data.length());
}

// from RTTcpNoTimeout
void LRTTransferSession::OnRecvData(const char*pData, int nLen)
{
    if (!pData) {
        return;
    }
    RTJSBuffer::RecvData(pData, nLen);
}

void LRTTransferSession::OnRecvMessage(const char*message, int nLen)
{
    RTTransfer::DoProcessData(message, nLen);
}

// process seqn read
void LRTTransferSession::OnWakeupEvent(const char*pData, int nLen)
{
    LI("LRTTransferSession::OnWakeupEvent was called\n");
    if (m_queueSeqnMsg.size()==0) return;
    bool hasData = false;
    for(int i=0;i<MSG_PACKED_ONCE_NUM;++i)
    {
        if (m_queueSeqnMsg.size()>0)
        {
            hasData = true;
            m_packedSeqnMsg.mutable_msgs(i)->ParseFromString(m_queueSeqnMsg.front());
            {
                 OSMutexLocker locker(&m_mutexQueueSeqn);
                 m_queueSeqnMsg.pop();
            }
        } else {
             break;
        }
    }
    if (hasData)
    {
        pms::RelayMsg rmsg;
        rmsg.set_svr_cmds(pms::EServerCmd::CSYNCSEQN);
        rmsg.set_content(m_packedSeqnMsg.SerializeAsString());

        pms::TransferMsg tmsg;
        tmsg.set_type(pms::ETransferType::TREAD_REQUEST);
        tmsg.set_flag(pms::ETransferFlag::FNOACK);
        tmsg.set_priority(pms::ETransferPriority::PNORMAL);
        tmsg.set_content(rmsg.SerializeAsString());

        LI("LRTTransferSession::OnWakeupEvent send seqn read request to logical server\n");
        this->SendTransferData(tmsg.SerializeAsString());
        for(int i=0;i<MSG_PACKED_ONCE_NUM;++i)
        {
             m_packedSeqnMsg.mutable_msgs(i)->Clear();
        }
    }
    if (m_queueSeqnMsg.size()>0)
    {
        this->Signal(Task::kWakeupEvent);
    }
}

// process new msgs
void LRTTransferSession::OnPushEvent(const char*pData, int nLen)
{
    LI("LRTTransferSession::OnPushEvent was called\n");
    if (m_queueNewMsg.size()==0) return;
    bool hasData = false;
    for(int i=0;i<MSG_PACKED_ONCE_NUM;++i)
    {
        if (m_queueNewMsg.size()>0)
        {
            hasData = true;
            m_packedNewMsg.mutable_msgs(i)->ParseFromString(m_queueNewMsg.front());
            {
                 OSMutexLocker locker(&m_mutexQueueNew);
                 m_queueNewMsg.pop();
            }
        } else {
            break;
        }
    }
    if (hasData)
    {
        pms::RelayMsg rmsg;
        rmsg.set_svr_cmds(pms::EServerCmd::CNEWMSG);
        rmsg.set_content(m_packedNewMsg.SerializeAsString());

        pms::TransferMsg tmsg;
        tmsg.set_type(pms::ETransferType::TWRITE_REQUEST);
        tmsg.set_flag(pms::ETransferFlag::FNOACK);
        tmsg.set_priority(pms::ETransferPriority::PNORMAL);
        tmsg.set_content(rmsg.SerializeAsString());

        LI("LRTTransferSession::OnPushEvent send seqn write request to logical server\n");
        this->SendTransferData(tmsg.SerializeAsString());
        for(int i=0;i<MSG_PACKED_ONCE_NUM;++i)
        {
             m_packedNewMsg.mutable_msgs(i)->Clear();
        }
    }
    if (m_queueNewMsg.size()>0)
    {
        this->Signal(Task::kPushEvent);
    }
}

// process data read
void LRTTransferSession::OnTickEvent(const char*pData, int nLen)
{
    LI("LRTTransferSession::OnTickEvent was called, m_queueDataMsg.size:%d, g_on_ticket_time:%d\n"\
            , m_queueDataMsg.size(), ++g_on_ticket_time);
    if (m_queueDataMsg.size()==0) return;
    bool hasData = false;
    for(int i=0;i<MSG_PACKED_ONCE_NUM;++i)
    {
        LI("LRTTransferSession::OnTickEvent loop m_queueDataMsg.size:%d\n", m_queueDataMsg.size());
        if (m_queueDataMsg.size()>0)
        {
            hasData = true;
            m_packedDataMsg.mutable_msgs(i)->ParseFromString(m_queueDataMsg.front());
            {
                 OSMutexLocker locker(&m_mutexQueueData);
                 m_queueDataMsg.pop();
            }
            LI("OnTickEvent read request sequence:%lld, ruserid:%s, m_queue.size:%d\n"\
                    , m_packedDataMsg.msgs(i).sequence()\
                    , m_packedDataMsg.msgs(i).ruserid().c_str()\
                    , m_queueDataMsg.size());
        } else {
            break;
        }
    }
    if (hasData)
    {
        pms::RelayMsg rmsg;
        rmsg.set_svr_cmds(pms::EServerCmd::CSYNCDATA);
        rmsg.set_content(m_packedDataMsg.SerializeAsString());

        pms::TransferMsg tmsg;
        tmsg.set_type(pms::ETransferType::TREAD_REQUEST);
        tmsg.set_flag(pms::ETransferFlag::FNOACK);
        tmsg.set_priority(pms::ETransferPriority::PNORMAL);
        tmsg.set_content(rmsg.SerializeAsString());

        this->SendTransferData(tmsg.SerializeAsString());
        for(int i=0;i<MSG_PACKED_ONCE_NUM;++i)
        {
             m_packedDataMsg.mutable_msgs(i)->Clear();
        }
    }
    if (m_queueDataMsg.size()>0)
    {
        this->Signal(Task::kIdleEvent);
    }
}


// from RTTransfer

void LRTTransferSession::OnTransfer(const std::string& str)
{
    RTTcpNoTimeout::SendTransferData(str.c_str(), (int)str.length());
}

void LRTTransferSession::OnMsgAck(pms::TransferMsg& tmsg)
{
#if DEF_PROTO
    pms::TransferMsg ack_msg;
    ack_msg.set_type(tmsg.type());
    ack_msg.set_flag(pms::ETransferFlag::FACK);
    ack_msg.set_priority(tmsg.priority());

    OnTransfer(ack_msg.SerializeAsString());
#else
    LE("not define DEF_PROTO\n");
#endif
}

void LRTTransferSession::OnTypeConn(const std::string& str)
{
#if DEF_PROTO
    pms::ConnMsg c_msg;
    if (!c_msg.ParseFromString(str)) {
        LE("OnTypeConn c_msg ParseFromString error\n");
        return;
    }
    LI("OnTypeConn connmsg--->:\n");

    if ((c_msg.conn_tag() == pms::EConnTag::THI)) {
        // when other connect to ME:
        // send the transfersessionid and ModuleId to other
        pms::TransferMsg t_msg;
        std::string trid;
        GenericSessionId(trid);
        m_transferSessId = trid;

        c_msg.set_tr_module(pms::ETransferModule::MLIVE);
        c_msg.set_conn_tag(pms::EConnTag::THELLO);
        c_msg.set_transferid(m_transferSessId);
        //send self Module id to other
        c_msg.set_moduleid(LRTConnManager::Instance().RTLiveId());

        t_msg.set_type(pms::ETransferType::TCONN);
        //this is for transfer
        t_msg.set_flag(pms::ETransferFlag::FNEEDACK);
        t_msg.set_priority(pms::ETransferPriority::PHIGH);
        t_msg.set_content(c_msg.SerializeAsString());

        std::string s = t_msg.SerializeAsString();
        SendTransferData(s.c_str(), (int)s.length());
    } else if ((c_msg.conn_tag() == pms::EConnTag::THELLO)) {
        // when ME connector to other:
        // store other's transfersessionid and other's moduleId
        if (c_msg.transferid().length()>0) {
            m_transferSessId = c_msg.transferid();
            {
                LRTConnManager::ModuleInfo* pmi = new LRTConnManager::ModuleInfo();
                if (pmi) {
                    pmi->flag = 1;
                    pmi->othModuleType = c_msg.tr_module();
                    pmi->othModuleId = m_transferSessId;
                    pmi->pModule = this;
                    //bind session and transfer id
                    LRTConnManager::Instance().AddModuleInfo(pmi, m_transferSessId);
                    //store which moudle connect to this connector
                    //c_msg._moduleid:store other's module id
                    LI("store other connector moduleid:%s, transfersessionid:%s\n", c_msg.moduleid().c_str(), m_transferSessId.c_str());
                    LRTConnManager::Instance().AddTypeModuleSession(c_msg.tr_module(), c_msg.moduleid(), m_transferSessId);
                } else {
                    LE("new ModuleInfo error!!!\n");
                }
            }

            pms::TransferMsg t_msg;

            c_msg.set_tr_module(pms::ETransferModule::MLIVE);
            c_msg.set_conn_tag(pms::EConnTag::THELLOHI);
            c_msg.set_transferid(m_transferSessId);
            //send self Module id to other
            c_msg.set_moduleid(LRTConnManager::Instance().RTLiveId());

            t_msg.set_type(pms::ETransferType::TCONN);
            //this is for transfer
            t_msg.set_flag(pms::ETransferFlag::FNEEDACK);
            t_msg.set_priority(pms::ETransferPriority::PHIGH);
            t_msg.set_content(c_msg.SerializeAsString());

            std::string s = t_msg.SerializeAsString();
            SendTransferData(s.c_str(), (int)s.length());
        } else {
            LE("Connection id:%s error!!!\n", c_msg.transferid().c_str());
        }
    } else if ((c_msg.conn_tag() == pms::EConnTag::THELLOHI)) {
        // when other connect to ME:
        if (m_transferSessId.compare(c_msg.transferid()) == 0) {
            LRTConnManager::ModuleInfo* pmi = new LRTConnManager::ModuleInfo();
            if (pmi) {
                pmi->flag = 1;
                pmi->othModuleType = c_msg.tr_module();
                pmi->othModuleId = m_transferSessId;
                pmi->pModule = this;
                //bind session and transfer id
                LRTConnManager::Instance().AddModuleInfo(pmi, m_transferSessId);
                //store which moudle connect to this connector
                //store other module id
                LI("store module type:%d, moduleid:%s, transfersessid:%s\n", (int)c_msg.tr_module(), c_msg.moduleid().c_str(), m_transferSessId.c_str());
                LRTConnManager::Instance().AddTypeModuleSession(c_msg.tr_module(), c_msg.moduleid(), m_transferSessId);
            } else {
                LE("new ModuleInfo error!!!!\n");
            }
        }
    }  else if (c_msg.conn_tag() == pms::EConnTag::TKEEPALIVE) {
        RTTcpNoTimeout::UpdateTimer();
    } else {
        LE("%s invalid msg tag\n", __FUNCTION__);
    }
#else
    LE("not define DEF_PROTO\n");
#endif
}

// recv msg from connector
void LRTTransferSession::OnTypeTrans(const std::string& str)
{
    //LI("%s was called, str:%s\n", __FUNCTION__, str.c_str());
    LRTRTLiveManager::Instance().RecvRequestCounter();
    pms::RelayMsg r_msg;
    if (!r_msg.ParseFromString(str)) return;
    LI("LRTTransferSession::OnTypeTrans srv:cmd:%d\n", r_msg.svr_cmds());
    if (r_msg.svr_cmds() == pms::EServerCmd::CSNDMSG)
    {
        pms::Entity e_msg;
        if (e_msg.ParseFromString(r_msg.content()))
        {
            LI("LRTTransferSession::OnTypeTrans ParseFromString ok\n");
        } else {
            LI("LRTTransferSession::OnTypeTrans ParseFromString error\n");
            assert(false);

        }
        LI("LRTTransferSession::OnTypeTrans Entity msg flag is:%d, touser.size:%d\n"\
                , e_msg.msg_flag(), e_msg.usr_toto().users_size());

        if (e_msg.msg_flag()==pms::EMsgFlag::FGROUP)
        {
            LI("LRTTransferSession::OnTypeTrans GROUP sndmsggroup usr_from:%s, msg_flag:%d, groupid:%s\n"\
                    , e_msg.usr_from().c_str(), e_msg.msg_flag(), e_msg.rom_id().c_str());
            assert(e_msg.rom_id().length()>0);

            LI("the sender move to module sync data notify!!!!!!\n");
#if 0
#endif

            // if the msg send to group
            // here change the userid and groupid
            // in order to store msg by userid
            // and tell group module the groupid
            // when recv response, change the userid and groupid back
            pms::StorageMsg recver;
            recver.set_rsvrcmd(pms::EServerCmd::CNEWMSG);
            recver.set_tsvrcmd(pms::EServerCmd::CNEWMSGSEQN);
            recver.set_mtype(std::to_string(e_msg.msg_type()));
            recver.set_ispush(std::to_string(e_msg.ispush()));
            recver.set_storeid(e_msg.rom_id());
            recver.set_ruserid(e_msg.usr_from());
            recver.set_mrole(pms::EMsgRole::RRECVER);
            recver.set_mflag(pms::EMsgFlag::FGROUP);
            recver.set_groupid(e_msg.rom_id());
            recver.set_version(e_msg.version());
            // store the Entity to redis
            recver.set_content(r_msg.content());
            LRTConnManager::Instance().PushNewMsg2Queue(recver.SerializeAsString());

        } else if (e_msg.msg_flag()==pms::EMsgFlag::FSINGLE)
        {
            LI("LRTTransferSession::OnTypeTrans SINGLE sndsinglemsg usr_from:%s, msg_flag:%d, usr_toto.size:%d\n"\
                    , e_msg.usr_from().c_str(), e_msg.msg_flag(), e_msg.usr_toto().users_size());
            assert(e_msg.usr_toto().users_size()==1);

            pms::StorageMsg sender;
            sender.set_rsvrcmd(pms::EServerCmd::CNEWMSG);
            sender.set_tsvrcmd(pms::EServerCmd::CNEWMSGSEQN);
            sender.set_mtype(std::to_string(e_msg.msg_type()));
            sender.set_ispush(std::to_string(e_msg.ispush()));
            sender.set_storeid(e_msg.usr_from());
            sender.set_ruserid(e_msg.usr_from());
            sender.set_mrole(pms::EMsgRole::RSENDER);
            sender.set_mflag(pms::EMsgFlag::FSINGLE);
            sender.set_version(e_msg.version());
            // store the Entity to redis
            sender.set_content(r_msg.content());
            LRTConnManager::Instance().PushNewMsg2Queue(sender.SerializeAsString());

            pms::StorageMsg recver;
            recver.set_rsvrcmd(pms::EServerCmd::CNEWMSG);
            recver.set_tsvrcmd(pms::EServerCmd::CNEWMSGSEQN);
            recver.set_mtype(std::to_string(e_msg.msg_type()));
            recver.set_ispush(std::to_string(e_msg.ispush()));
            recver.set_storeid(e_msg.usr_toto().users(0));
            recver.set_ruserid(e_msg.usr_toto().users(0));
            recver.set_mrole(pms::EMsgRole::RRECVER);
            recver.set_mflag(pms::EMsgFlag::FSINGLE);
            recver.set_version(e_msg.version());
            // store the Entity to redis
            recver.set_content(r_msg.content());
            LRTConnManager::Instance().PushNewMsg2Queue(recver.SerializeAsString());
        } else if (e_msg.msg_flag()==pms::EMsgFlag::FMULTI)
        {
            LI("LRTTransferSession::OnTypeTrans MULTI sndmultimsg usr_from:%s, msg_flag:%d, usr_toto.size:%d\n"\
                    , e_msg.usr_from().c_str(), e_msg.msg_flag(), e_msg.usr_toto().users_size());
            assert(e_msg.usr_toto().users_size()>=1);

            pms::StorageMsg sender;
            sender.set_rsvrcmd(pms::EServerCmd::CNEWMSG);
            sender.set_tsvrcmd(pms::EServerCmd::CNEWMSGSEQN);
            sender.set_mtype(std::to_string(e_msg.msg_type()));
            sender.set_ispush(std::to_string(e_msg.ispush()));
            sender.set_storeid(e_msg.usr_from());
            sender.set_ruserid(e_msg.usr_from());
            sender.set_mrole(pms::EMsgRole::RSENDER);
            sender.set_mflag(pms::EMsgFlag::FSINGLE);
            sender.set_version(e_msg.version());
            // store the Entity to redis
            sender.set_content(r_msg.content());
            LRTConnManager::Instance().PushNewMsg2Queue(sender.SerializeAsString());

            for (int i=0;i<e_msg.usr_toto().users_size();++i) {
                pms::StorageMsg recver;
                recver.set_rsvrcmd(pms::EServerCmd::CNEWMSG);
                recver.set_tsvrcmd(pms::EServerCmd::CNEWMSGSEQN);
                recver.set_mtype(std::to_string(e_msg.msg_type()));
                recver.set_ispush(std::to_string(e_msg.ispush()));
                recver.set_storeid(e_msg.usr_toto().users(i));
                recver.set_ruserid(e_msg.usr_toto().users(i));
                recver.set_mrole(pms::EMsgRole::RRECVER);
                recver.set_mflag(pms::EMsgFlag::FSINGLE);
                recver.set_version(e_msg.version());
                // store the Entity to redis
                recver.set_content(r_msg.content());
                LRTConnManager::Instance().PushNewMsg2Queue(recver.SerializeAsString());
            }
        } else {
            LI("LRTTransferSession::OnTypeTrans Entity msg flag:%d not handle\n\n", e_msg.msg_flag());
        }
    } else if (r_msg.svr_cmds() == pms::EServerCmd::CCREATESEQN)
    {
        LI("LRTTransferSession::OnTypeTrans CREATESEQN  was called\n");
        LRTConnManager::Instance().PushNewMsg2Queue(r_msg.content());
    } else if (r_msg.svr_cmds() == pms::EServerCmd::CDELETESEQN)
    {
        LI("LRTTransferSession::OnTypeTrans CDELETESEQN  was called\n");
        LRTConnManager::Instance().PushNewMsg2Queue(r_msg.content());
    } else if (r_msg.svr_cmds() == pms::EServerCmd::CSYNCSEQN)
    {
        pms::StorageMsg s_msg;
        if (!s_msg.ParseFromString(r_msg.content())) return;
        LI("SYNC SEQN sequence:%lld, ruserid:%s\n", s_msg.sequence(), s_msg.ruserid().c_str());
        LRTConnManager::Instance().PushSeqnReq2Queue(r_msg.content());
    } else if (r_msg.svr_cmds() == pms::EServerCmd::CSYNCDATA)
    {
        pms::StorageMsg s_msg;
        if (!s_msg.ParseFromString(r_msg.content())) return;
        LI("SYNC DATA sequence:%lld, ruserid:%s\n"\
                , s_msg.sequence(), s_msg.ruserid().c_str());

        // first set sync seqn for data cmd, send to sync seqn
        LRTConnManager::Instance().PushDataReq2Queue(r_msg.content());
    } else if (r_msg.svr_cmds() == pms::EServerCmd::CUPDATESETTING)
    {
        pms::Setting s_set;
        if (!s_set.ParseFromString(r_msg.content())) return;
        LI("UPDATE SETTING usr_from:%s, set_type:%lld, json_cont:%s, version:%s\n", s_set.usr_from().c_str(), s_set.set_type(), s_set.json_cont().c_str(), s_set.version().c_str());
    }
}

// for write
void LRTTransferSession::OnTypeQueue(const std::string& str)
{
    LI("%s was called\n", __FUNCTION__);
    pms::PackedStoreMsg store;
    if (!store.ParseFromString(str)) return;
    for(int i=0;i<store.msgs_size();++i)
    {
        if (store.msgs(i).ruserid().length()==0) break;
        LI("OnTypeQueue sequence:%lld, ruserid:%s\n"\
                , store.msgs(i).sequence()\
                , store.msgs(i).ruserid().c_str());

        pms::TransferMsg t_msg;
        pms::RelayMsg r_msg;
        pms::MsgRep resp;

        // set response
        LI("LRTTransferSession::OnTypeQueue --->store.rsvrcmd:%d\n", store.msgs(i).rsvrcmd());
        LI("!!!!!!!!!!!!!!LRTTransferSession::OnTypeQueue --->store.ruserid:%s, msgid:%s, seqn:%lld, mflag:%d, mrole:%d\n"\
                , store.msgs(i).ruserid().c_str()\
                , store.msgs(i).msgid().c_str()\
                , store.msgs(i).sequence()\
                , store.msgs(i).mflag()\
                , store.msgs(i).mrole());
        if (store.msgs(i).rsvrcmd()==pms::EServerCmd::CNEWMSG)
        {
            pms::Entity entity;
            if (store.msgs(i).mflag()==pms::EMsgFlag::FGROUP)
            {
                if (store.msgs(i).mrole()==pms::EMsgRole::RSENDER)
                {
                    // set response
                    LI("group msg sender notify move to module notify sync data!!!!!!!!\n");
#if 0
#endif
                } else if (store.msgs(i).mrole()==pms::EMsgRole::RRECVER)
                {
                    // group to notify members to sync seqn from this group
                    // the store.msgs(i).groupid should be entity's userid
                    // the store.msgs(i).userid should be entity's rom_id
                    // no long sync data directly
                    pms::StorageMsg st;
                    pms::TransferMsg tmsg;

                    //group11111
                    st.set_rsvrcmd(pms::EServerCmd::CGROUPNOTIFY);
                    st.set_tsvrcmd(pms::EServerCmd::CGROUPNOTIFY);
                    st.set_mflag(pms::EMsgFlag::FGROUP);
                    st.set_sequence(store.msgs(i).sequence());
                    LI("+++++++++++++++++++++++New Msg st.sequence:%lld, store.sequence:%lld, version:%s\n", st.sequence(), store.msgs(i).sequence(), store.msgs(i).version().c_str());
                    st.set_groupid(store.msgs(i).groupid());
                    st.set_storeid(store.msgs(i).storeid());
                    st.set_ruserid(store.msgs(i).ruserid());

                    // add mtype and push
                    st.set_mtype(store.msgs(i).mtype());
                    st.set_ispush(store.msgs(i).ispush());
                    st.set_version(store.msgs(i).version());

                    resp.set_svr_cmds(pms::EServerCmd::CGROUPNOTIFY);
                    resp.set_rsp_code(0);
                    resp.set_mod_type(pms::EModuleType::TLIVE);
                    resp.set_rsp_cont(st.SerializeAsString());

                    tmsg.set_type(pms::ETransferType::TTRANS);
                    tmsg.set_content(resp.SerializeAsString());

                    std::string s = tmsg.SerializeAsString();
                    LI("LRTTransferSession::OnTypeQueue --->group msg seqn:%lld, from who ruserid:%s, to groupid:%s\n\n"\
                            , store.msgs(i).sequence(), st.ruserid().c_str(), st.groupid().c_str());
                    //TODO:
                    //how to let others server connect to here, find the special group users relayer
                    //send to
                    const std::string sn("wensiwensi");
                    if (LRTConnManager::Instance().SendToGroupModule(sn, s))
                    {
                         LI("LRTTransferSession::OnTypeQueue dispatch msg ok\n");
                    } else {
                         LI("LRTTransferSession::OnTypeQueue noooooooooot dispatch msg ok\n");
                    }

                    continue;
                } else {

                    LI("LRTTransferSession::OnTypeQueue --->store.mrole:%d not handle\n\n", store.msgs(i).mrole());
                }
            } else if (store.msgs(i).mflag()==pms::EMsgFlag::FSINGLE)
            {
                //pms::StorageMsg resp_store;
                if (store.msgs(i).mrole()==pms::EMsgRole::RSENDER)
                {
                    // after generate new msg, notify sender sync seqn

                    // set response
                    LI("LRTTransferSession::OnTypeQueue notify single sender sync seqn\n");
                    resp.set_svr_cmds(pms::EServerCmd::CSNTFSEQN);
                    resp.set_mod_type(pms::EModuleType::TLIVE);
                    resp.set_rsp_code(store.msgs(i).result());
                    resp.set_rsp_cont(store.msgs(i).SerializeAsString());

                    // set relay
                    r_msg.set_svr_cmds(pms::EServerCmd::CSNDMSG);
                    r_msg.set_tr_module(pms::ETransferModule::MLIVE);
                    r_msg.set_connector("");
                    r_msg.set_content(resp.SerializeAsString());

                    LI("============>>>>>>>>single sender new msg sequence:%lld\n\n", store.msgs(i).sequence());
                    // this is for single sender notification push setting
                    r_msg.set_handle_cmd("push"); // set for push
                    r_msg.set_handle_mtype(store.msgs(i).mtype());
                    r_msg.set_handle_data(store.msgs(i).ispush());

                    pms::ToUser *pto = new pms::ToUser;
                    LI("LRTTransferSession::OnTypeQueue send response to usr:%s\n", store.msgs(i).ruserid().c_str());
                    pto->add_users()->assign(store.msgs(i).ruserid());
                    r_msg.set_allocated_touser(pto);
                } else if (store.msgs(i).mrole()==pms::EMsgRole::RRECVER)
                {
                    // after generate new msg, notify recver sync seqn, no long sync data directly

                    // set response
                    LI("LRTTransferSession::OnTypeQueue notify single recver sync data\n");
                    resp.set_svr_cmds(pms::EServerCmd::CSNTFSEQN);
                    resp.set_mod_type(pms::EModuleType::TLIVE);
                    resp.set_rsp_code(0);
                    resp.set_rsp_cont(store.msgs(i).SerializeAsString());

                    // set relay
                    r_msg.set_svr_cmds(pms::EServerCmd::CSNDMSG);
                    r_msg.set_tr_module(pms::ETransferModule::MLIVE);
                    r_msg.set_connector("");
                    r_msg.set_content(resp.SerializeAsString());

                    LI("============>>>>>>>>single recver new msg sequence:%lld\n\n", store.msgs(i).sequence());
                    // this is for single recver notification push setting
                    r_msg.set_handle_cmd("push"); // set for push
                    r_msg.set_handle_mtype(store.msgs(i).mtype());
                    r_msg.set_handle_data(store.msgs(i).ispush());

                    pms::ToUser *pto = new pms::ToUser;
                    LI("LRTTransferSession::OnTypeQueue send response to usr:%s\n", store.msgs(i).ruserid().c_str());
                    // although this is recver, but when gen new msg, the ruserid was already set to the receiver
                    pto->add_users()->assign(store.msgs(i).ruserid());
                    r_msg.set_allocated_touser(pto);
                } else {

                    LI("LRTTransferSession::OnTypeQueue --->store.mrole:%d not handle\n\n", store.msgs(i).mrole());
                }
            } else {
                LI("LRTTransferSession::OnTypeQueue --->store.mflag:%d not handle\n\n", store.msgs(i).mflag());
            }

            // set transfer
            t_msg.set_type(pms::ETransferType::TQUEUE);
            t_msg.set_content(r_msg.SerializeAsString());
            std::string response = t_msg.SerializeAsString();
            LRTConnManager::Instance().SendTransferData("", "", response);
        } else if (store.msgs(i).rsvrcmd()==pms::EServerCmd::CCREATESEQN)
        {
            resp.set_svr_cmds(store.msgs(i).rsvrcmd());
            resp.set_mod_type(pms::EModuleType::TLIVE);
            resp.set_rsp_code(0);
            resp.set_rsp_cont(store.msgs(i).SerializeAsString());

            pms::TransferMsg tmsg;
            tmsg.set_type(pms::ETransferType::TTRANS);
            tmsg.set_content(resp.SerializeAsString());
            std::string s = tmsg.SerializeAsString();
            LI("LRTTransferSession::OnTypeQueue --->create group seqn rsrvcmd:%d from who ruserid:%s,by client:%s to groupid:%s\n\n"\
                    , store.msgs(i).rsvrcmd()\
                    , store.msgs(i).ruserid().c_str()\
                    , store.msgs(i).groupid().c_str()\
                    , store.msgs(i).storeid().c_str());
            //TODO:
            //how to let others server connect to here, find the special group users relayer
            //send to
            const std::string sn("wensiwensi");
            if (LRTConnManager::Instance().SendToGroupModule(sn, s))
            {
                LI("LRTTransferSession::OnTypeQueue cmd create dispatch msg ok\n");
            } else {
                LI("LRTTransferSession::OnTypeQueue cmd create noooooooooot dispatch msg ok\n");
            }
        } else if (store.msgs(i).rsvrcmd()==pms::EServerCmd::CDELETESEQN)
        {
            // direct send back to group client
            // no need to relay by queue
            resp.set_svr_cmds(store.msgs(i).rsvrcmd());
            resp.set_mod_type(pms::EModuleType::TLIVE);
            resp.set_rsp_code(0);
            resp.set_rsp_cont(store.msgs(i).SerializeAsString());

            pms::TransferMsg tmsg;
            tmsg.set_type(pms::ETransferType::TTRANS);
            tmsg.set_content(resp.SerializeAsString());
            std::string s = tmsg.SerializeAsString();
            LI("LRTTransferSession::OnTypeQueue --->delete group seqn rsrvcmd:%d from who ruserid:%s,by client:%s to groupid:%s\n\n"\
                    , store.msgs(i).rsvrcmd()\
                    , store.msgs(i).ruserid().c_str()\
                    , store.msgs(i).groupid().c_str()\
                    , store.msgs(i).storeid().c_str());
            //TODO:
            //how to let others server connect to here, find the special group users relayer
            //send to
            const std::string sn("wensiwensi");
            if (LRTConnManager::Instance().SendToGroupModule(sn, s))
            {
                LI("LRTTransferSession::OnTypeQueue cmd delete dispatch msg ok\n");
            } else {
                LI("LRTTransferSession::OnTypeQueue cmd delete noooooooooot dispatch msg ok\n");
            }
        } else {
            LI("LRTTransferSession::OnTypeQueue store.srv:cmd:%d not handle\n", store.msgs(i).rsvrcmd());
        }
    }
}

// for read get data or seqn for storage or sequence server
void LRTTransferSession::OnTypeDispatch(const std::string& str)
{
    LI("%s was called\n", __FUNCTION__);
    pms::PackedStoreMsg store;
    if (!store.ParseFromString(str)) return;
    for(int i=0;i<store.msgs_size();++i)
    {
        if (store.msgs(i).ruserid().length()==0)
        {
            LI("%s store.msgs(%d).ruserid length is 0\n", __FUNCTION__, i);
            break;
        }

        LI("OnTypeDispatch sequence:%lld, ruserid:%s\n"\
                , store.msgs(i).sequence()\
                , store.msgs(i).ruserid().c_str());

        pms::TransferMsg t_msg;
        pms::RelayMsg r_msg;
        pms::MsgRep resp;

        // set response
        LI("LRTTransferSession::OnTypeDispatch --->store.rsvrcmd:%d\n", store.msgs(i).rsvrcmd());
        LI("LRTTransferSession::OnTypeDispatch --->store.ruserid:%s, msgid:%s, seqn:%lld, cont.length:%d\n\n"\
                , store.msgs(i).ruserid().c_str()\
                , store.msgs(i).msgid().c_str()\
                , store.msgs(i).sequence()\
                , store.msgs(i).content().length());
        switch(store.msgs(i).rsvrcmd()) {
            case pms::EServerCmd::CSYNCSEQN:
                {
                    LI("LRTTransferSession::OnTypeDispatch CSYNCSEQN, seqn:%lld, maxseqn:%lld, result:%d\n", store.msgs(i).sequence(), store.msgs(i).maxseqn(), store.msgs(i).result());
                    if (store.msgs(i).result()!=0)
                    {
                        LE("LRTTransferSession::OnTypeDispatch store.msgs.result():%d, is not equal 0000000000, continue\n", store.msgs(i).result());
                        continue;
                    }
                    resp.set_svr_cmds(store.msgs(i).rsvrcmd());
                    resp.set_mod_type(pms::EModuleType::TLIVE);
                    resp.set_rsp_code(0);
                    resp.set_rsp_cont(store.msgs(i).SerializeAsString());

                    // set relay
                    r_msg.set_svr_cmds(store.msgs(i).rsvrcmd());
                    r_msg.set_tr_module(pms::ETransferModule::MLIVE);
                    r_msg.set_cont_module(pms::EModuleType::TLIVE); // in connector, check get enablepush
                    r_msg.set_connector("");
                    r_msg.set_content(resp.SerializeAsString());
                    pms::ToUser *pto = new pms::ToUser;
                    pto->add_users()->assign(store.msgs(i).ruserid());
                    r_msg.set_allocated_touser(pto);

                    // set transfer
                    t_msg.set_type(pms::ETransferType::TQUEUE);
                    t_msg.set_content(r_msg.SerializeAsString());
                    std::string response = t_msg.SerializeAsString();
                    LRTConnManager::Instance().SendTransferData("", "", response);

                }
                break;
            case pms::EServerCmd::CSYNCDATA:
            case pms::EServerCmd::CSYNCGROUPDATA:
            case pms::EServerCmd::CSYNCONEDATA:
            case pms::EServerCmd::CSYNCONEGROUPDATA:
                {
                    LI("LRTTransferSession::OnTypeDispatch CSYNCXXXXDATA seqn:%lld, maxseqn:%lld, store.ruserid:%s, store.storeid:%s, store.groupid:%s\n"\
                            , store.msgs(i).sequence()\
                            , store.msgs(i).maxseqn()\
                            , store.msgs(i).ruserid().c_str()\
                            , store.msgs(i).storeid().c_str()\
                            , store.msgs(i).groupid().c_str());
                    resp.set_svr_cmds(store.msgs(i).rsvrcmd());
                    resp.set_mod_type(pms::EModuleType::TLIVE);
                    resp.set_rsp_code(0);
                    resp.set_rsp_cont(store.msgs(i).SerializeAsString());
                    LI("LRTTransferSession::OnTypeDispatch CSYNCXXXXDATA after change seqn:%lld, maxseqn:%lld, store.ruserid:%s, store.groupid:%s\n"\
                            , store.msgs(i).sequence()\
                            , store.msgs(i).maxseqn()\
                            , store.msgs(i).ruserid().c_str()\
                            , store.msgs(i).groupid().c_str());

                    // set relay
                    r_msg.set_svr_cmds(store.msgs(i).rsvrcmd());
                    r_msg.set_tr_module(pms::ETransferModule::MLIVE);
                    r_msg.set_connector("");
                    r_msg.set_content(resp.SerializeAsString());
                    // read sync data no need push anymore!!!!!!

                    pms::ToUser *pto = new pms::ToUser;
                    pto->add_users()->assign(store.msgs(i).ruserid());
                    r_msg.set_allocated_touser(pto);

                    // set transfer
                    t_msg.set_type(pms::ETransferType::TQUEUE);
                    t_msg.set_content(r_msg.SerializeAsString());
                    std::string response = t_msg.SerializeAsString();
                    LRTConnManager::Instance().SendTransferData("", "", response);
                }
                break;
            case pms::EServerCmd::CPGETDATA:
                {
                    // send data to pusher
                    LI("LRTTransferSession::OnTypeDispatch !!!!!CPGETDATA seqn:%lld, maxseqn:%lld, store.ruserid:%s, store.storeid:%s, store.groupid:%s\n"\
                            , store.msgs(i).sequence()\
                            , store.msgs(i).maxseqn()\
                            , store.msgs(i).ruserid().c_str()\
                            , store.msgs(i).storeid().c_str()\
                            , store.msgs(i).groupid().c_str());

                    if (store.msgs(i).result()!=0)
                    {
                        LE("LRTTransferSession::OnTypeDispatch store.msgs.result():%d, is not equal 0000000000, continue\n", store.msgs(i).result());
                        continue;
                    }
                    resp.set_svr_cmds(store.msgs(i).rsvrcmd());
                    resp.set_mod_type(pms::EModuleType::TLIVE);
                    resp.set_rsp_code(0);
                    resp.set_rsp_cont(store.msgs(i).SerializeAsString());
                    LI("LRTTransferSession::OnTypeDispatch !!!!!CPGETDATA after change seqn:%lld, maxseqn:%lld, store.ruserid:%s, store.groupid:%s\n"\
                            , store.msgs(i).sequence()\
                            , store.msgs(i).maxseqn()\
                            , store.msgs(i).ruserid().c_str()\
                            , store.msgs(i).groupid().c_str());


                    pms::TransferMsg tmsg;

                    tmsg.set_type(pms::ETransferType::TDISPATCH);
                    tmsg.set_content(resp.SerializeAsString());

                    std::string s = tmsg.SerializeAsString();
                    LI("LRTTransferSession::OnTypeQueue --->group msg seqn:%lld, from who ruserid:%s, to groupid:%s\n\n"\
                            , store.msgs(i).sequence(), store.msgs(i).ruserid().c_str(), store.msgs(i).groupid().c_str());
                    //TODO:
                    //how to let others server connect to here, find the special group users relayer
                    //send to
                    const std::string sn("wensiwensi");
                    if (LRTConnManager::Instance().SendToPushModule(sn, s))
                    {
                        LI("LRTTransferSession::OnTypeDispatch dispatch msg send to pusher ok\n");
                    } else {
                        LI("LRTTransferSession::OnTypeDispatch noooooooooot dispatch msg ok\n");
                    }
                    continue;

                }
                break;
            default:
                LI("LRTTransferSession::OnTypeDispatch srv:cmd:%d not handle\n", r_msg.svr_cmds());
                break;
        }
    }
}

// this is for recving other module cmd msg, e.g. grouper, pusher
void LRTTransferSession::OnTypePush(const std::string& str)
{
   LI("%s was called\n", __FUNCTION__);
   LRTModuleConnTcp::DoProcessData(str.c_str(), str.length());
}

void LRTTransferSession::OnTypeTLogin(const std::string& str)
{
    LI("%s was called\n", __FUNCTION__);
}

void LRTTransferSession::OnTypeTLogout(const std::string& str)
{
    LI("%s was called\n", __FUNCTION__);
}


// from LRTModuleConnTcp
void LRTTransferSession::OnSyncSeqn(pms::EServerCmd cmd, pms::EModuleType module, const std::string& msg)
{
    LI("%s was called\n", __FUNCTION__);
}

void LRTTransferSession::OnSyncData(pms::EServerCmd cmd, pms::EModuleType module, const std::string& msg)
{
    LI("%s was called\n", __FUNCTION__);
}

void LRTTransferSession::OnGroupNotify(pms::EServerCmd cmd, pms::EModuleType module, const std::string& msg)
{
    // this request is coming from group module client
    LI("%s was called\n", __FUNCTION__);
    pms::PackedStoreMsg packed;
    if (!packed.ParseFromString(msg)) return;
    for(int i=0;i<packed.msgs_size();++i)
    {
        if (packed.msgs(i).ruserid().length()==0)break;
        LI("LRTTransferSession::OnGroupNotify ruserid:%s, groupid:%s, rsvrcmd:%d, mflag:%d, sequence:%lld, mtype:%s, ispush:%s\n\n"\
                , packed.msgs(i).ruserid().c_str()\
                , packed.msgs(i).groupid().c_str()\
                , packed.msgs(i).rsvrcmd()\
                , packed.msgs(i).mflag()\
                , packed.msgs(i).sequence()\
                , packed.msgs(i).mtype().c_str()\
                , packed.msgs(i).ispush().c_str());

        //group44444
        pms::TransferMsg t_msg;
        pms::RelayMsg r_msg;
        pms::MsgRep resp;
        resp.set_svr_cmds(pms::EServerCmd::CGROUPNOTIFY);
        resp.set_mod_type(pms::EModuleType::TLIVE);
        resp.set_rsp_code(0);
        resp.set_rsp_cont(packed.msgs(i).SerializeAsString());

        // set relay
        r_msg.set_svr_cmds(pms::EServerCmd::CGROUPNOTIFY);
        r_msg.set_tr_module(pms::ETransferModule::MLIVE);
        r_msg.set_cont_module(pms::EModuleType::TLIVE); // in connector, check get enablepush
        r_msg.set_connector("");
        r_msg.set_content(resp.SerializeAsString());

        LI("============>>>>>>>>grouper recver new msg sequence:%lld\n\n", packed.msgs(i).sequence());
        // this is for group notification push setting
        if (packed.msgs(i).ispush().compare("1")==0)
        {
            r_msg.set_handle_cmd("push"); // set for push
            r_msg.set_handle_mtype(packed.msgs(i).mtype());
            r_msg.set_handle_data(packed.msgs(i).ispush());
        }

        pms::ToUser *pto = new pms::ToUser;
        pto->add_users()->assign(packed.msgs(i).ruserid());
        r_msg.set_allocated_touser(pto);

        // set transfer
        t_msg.set_type(pms::ETransferType::TQUEUE);
        t_msg.set_content(r_msg.SerializeAsString());
        std::string response = t_msg.SerializeAsString();
        LRTConnManager::Instance().SendTransferData("", "", response);
    }
}

// get request form module pusher to get data
// here should send to logical to sync the data
void LRTTransferSession::OnPGetData(pms::EServerCmd cmd, pms::EModuleType module, const std::string& msg)
{
    LI("~~~~~PGETDATA~~~~%s was called, cmd:%d, msg.len:%d\n", __FUNCTION__, cmd, msg.length());

    pms::PackedStoreMsg packed;
    if (!packed.ParseFromString(msg))
    {
        LE("LRTTransferSession::OnPGetData packed ParseFromString error, so return\n");
        return;
    }
    for(int i=0;i<packed.msgs_size();++i)
    {
        if (packed.msgs(i).ruserid().length()==0) break;

        LI("!!!!!!!!!!!!!!LRTTransferSession::OnPGetData --->store.ruserid:%s, rsvrcmd:%d, tsvrcmd:%d, msgid:%s, seqn:%lld, mflag:%d, mrole:%d\n"\
                , packed.msgs(i).ruserid().c_str()\
                , packed.msgs(i).rsvrcmd()\
                , packed.msgs(i).tsvrcmd()\
                , packed.msgs(i).msgid().c_str()\
                , packed.msgs(i).sequence()\
                , packed.msgs(i).mflag()\
                , packed.msgs(i).mrole());

        LRTConnManager::Instance().PushDataReq2Queue(packed.msgs(i).SerializeAsString());
    }
}


void LRTTransferSession::OnCreateGroupSeqn(pms::EServerCmd cmd, pms::EModuleType module, const std::string& msg)
{
    // this request is coming from group module client
    LI("%s was called, cmd:%d, msg.len:%d\n", __FUNCTION__, cmd, msg.length());
    LRTConnManager::Instance().PushNewMsg2Queue(msg);
    pms::StorageMsg store;
    if (!store.ParseFromString(msg)) return;
    LI("LRTTransferSession::OnCreateGroupSeqn --->create group seqn rsrvcmd:%d from who ruserid:%s,by client:%s to groupid:%s\n\n"\
            , store.rsvrcmd()\
            , store.ruserid().c_str()\
            , store.groupid().c_str()\
            , store.storeid().c_str());
}

void LRTTransferSession::OnDeleteGroupSeqn(pms::EServerCmd cmd, pms::EModuleType module, const std::string& msg)
{
    // this request is coming from group module client
    LI("%s was called, cmd:%d, msg.len:%d\n", __FUNCTION__, cmd, msg.length());
    LRTConnManager::Instance().PushNewMsg2Queue(msg);
    pms::StorageMsg store;
    if (!store.ParseFromString(msg)) return;
    LI("LRTTransferSession::OnCreateGroupSeqn --->delete group seqn rsrvcmd:%d from who ruserid:%s,by client:%s to groupid:%s\n\n"\
            , store.rsvrcmd()\
            , store.ruserid().c_str()\
            , store.groupid().c_str()\
            , store.storeid().c_str());
}

void LRTTransferSession::OnResponse(const char*pData, int nLen)
{
    LI("%s was called\n", __FUNCTION__);
    RTTcpNoTimeout::SendTransferData(pData, nLen);
}

void LRTTransferSession::ConnectionDisconnected()
{
    if (m_transferSessId.length()>0) {
        m_connectingStatus = 0;
        LRTConnManager::Instance().TransferSessionLostNotify(m_transferSessId);
    }
}

////////////////////////////////////////////////////////
////////////////////private/////////////////////////////
////////////////////////////////////////////////////////

