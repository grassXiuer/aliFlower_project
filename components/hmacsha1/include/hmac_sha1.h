#ifndef __HMAC_SHA1_H__
#define __HMAC_SHA1_H__


/*
����˵����
msg-Ҫ���ܵ�����
msg_len-���ݳ���
digest-������ܽ��
key-��������
key_len-�������ӳ���
*/
extern void aliyun_iot_common_hmac_sha1(const char *msg, int msg_len, char *digest, const char *key, int key_len);


#endif /* __HMAC_SHA1_H__ */
