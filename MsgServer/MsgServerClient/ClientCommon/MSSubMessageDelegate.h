//
//  MSSubMessageDelegate.h
//  SdkMsgClientIos
//
//  Created by hp on 6/29/16.
//  Copyright © 2016 Dync. All rights reserved.
//

#ifndef MSSubMessageDelegate_h
#define MSSubMessageDelegate_h
#include "MSMessage.h"

class MSSubMessageDelegate {
public:
/**
 *  when the msg you send reached server this function will be called
 *
 *  params:
 *      msgId: the message you just send, this 'msgId' is got by invoke methods in MSMessageManager.h
 *      code: the code, usually it is 0
 */
    virtual void OnSendMessage(const std::string& msgId, int code) = 0;

/**
 *  when recv text msg this function will be called
 *  in each received msg, 'nickname', 'uiconurl', 'fromid', 'msgType' will be all setted
 *  besides, different msg will has different valid field
 *
 *  params:
 *      txtMsg: the message you received
 *
 *  'groupid' and 'content' will be setted
 *  'groupid' means this message belong which group
 *  'content' means the message content
 */
    virtual void OnRecvTxtMessage(MSMessage* txtMsg) = 0;

/**
 *  when recv self define msg this function will be called, not used now
 *  in each received msg, 'nickname', 'uiconurl', 'fromid', 'msgType' will be all setted
 *  besides, different msg will has different valid field
 *
 *  params:
 *      sdefMsg: the message you received
 */
    virtual void OnRecvSelfDefMessage(MSMessage* sdefMsg) = 0;

/**
 *  when hostId begin to live in grpId this function will be called
 *  in each received msg, 'nickname', 'uiconurl', 'fromid', 'msgType' will be all setted
 *  besides, different msg will has different valid field
 *
 *  params:
 *      livMsg: the message you received
 *
 *  'groupid' will be setted
 *  'groupid' means this message belong which group
 */
    virtual void OnNotifyLiveMessage(MSMessage* livMsg) = 0;

/**
 *  when hostId in grpId recv red-envelope this function will be called
 *  in each received msg, 'nickname', 'uiconurl', 'fromid', 'msgType' will be all setted
 *  besides, different msg will has different valid field
 *
 *  params:
 *      renMsg: the message you received
 *
 *  'groupid' and 'userid' and 'cash' and 'wishcont' will be setted
 *  'groupid' means this message belong which group
 *  'userid' means this red envelope is send to
 *  'cash" means the cash amount
 *  'wishcont' means the wish content
 */
    virtual void OnNotifyRedEnvelopeMessage(MSMessage* renMsg) = 0;

/**
 *  when userId was push to blacklist in grpId this function will be called
 *  in each received msg, 'nickname', 'uiconurl', 'fromid', 'msgType' will be all setted
 *  besides, different msg will has different valid field
 *
 *  params:
 *      blkMsg: the message you received
 *
 *  'groupid' and 'userid' will be setted
 *  'groupid' means this message belong which group
 *  'userid' means who will be pull blacklist, if userid equals your id, means you are pulled to blacklist
 *  if userid not equals your id, means you are notified to know
 */
    virtual void OnNotifyBlacklistMessage(MSMessage* blkMsg) = 0;

/**
 *  when userId was forbidden in grpId this function will be called
 *  in each received msg, 'nickname', 'uiconurl', 'fromid', 'msgType' will be all setted
 *  besides, different msg will has different valid field
 *
 *  params:
 *      fbdMsg: the message you received
 *
 *  'groupid' and 'userid'will be setted
 *  'groupid' means this message belong which group
 *  'userid' means who will be forbidden to talk, if userid equals your id, means you are forbidden to talk
 *  if userid not equals your id, means you are notified to know
 */
    virtual void OnNotifyForbiddenMessage(MSMessage* fbdMsg) = 0;

/**
 *  when userId in grpId was setted to be manager this function will be called
 *  in each received msg, 'nickname', 'uiconurl', 'fromid', 'msgType' will be all setted
 *  besides, different msg will has different valid field
 *
 *  params:
 *      mgrMsg: the message you received
 *
 *  'groupid' and 'userid' will be setted
 *  'groupid' means this message belong which group
 *  'userid' means who will be setted to be manager, if userid equals your id, means you are setted to be manager
 *  if userid not equals your id, means you are notified to know
 */
    virtual void OnNotifySettedMgrMessage(MSMessage* mgrMsg) = 0;

/**
 *  when this account was loggined on another device
 *  you will receive this notification
 *  what you need to do is invoke method 'delDelegateId' and 'uninMsgClient' in MSClientManager
 *
 *  params:
 *      code: the code
 */
    virtual void OnNotifyOtherLogin(int code) = 0;

};

#endif /* MSSubMessageDelegate_h */
