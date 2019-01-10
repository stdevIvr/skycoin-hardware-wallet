#include "fsm_impl.h"

#include <stdio.h>

#include <libopencm3/stm32/flash.h>


#include "trezor.h"
#include "fsm.h"
#include "messages.h"
#include "bip32.h"
#include "storage.h"
#include "rng.h"
#include "storage.h"
#include "oled.h"
#include "protect.h"
#include "pinmatrix.h"
#include "layout2.h"
#include "base58.h"
#include "reset.h"
#include "recovery.h"
#include "bip39.h"
#include "memory.h"
#include "usb.h"
#include "util.h"
#include "base58.h"
#include "gettext.h"
#include "skycoin_crypto.h"
#include "skycoin_check_signature.h"
#include "check_digest.h"

ErrCode_t fsm_msgGenerateMnemonicImpl(GenerateMnemonic* msg) {
 	CHECK_NOT_INITIALIZED_RET_ERR_CODE
	const char* mnemonic = mnemonic_generate(128);
	if (mnemonic == 0) {
		fsm_sendFailure(FailureType_Failure_ProcessError, _("Device could not generate a Mnemonic"));
		return ErrFailed;
	}
	if (!mnemonic_check(mnemonic)) {
		fsm_sendFailure(FailureType_Failure_DataError, _("Mnemonic with wrong checksum provided"));
		return ErrFailed;
	}
	storage_setMnemonic(mnemonic);
	storage_setNeedsBackup(true);
	storage_setPassphraseProtection(msg->has_passphrase_protection && msg->passphrase_protection);
	storage_update();
	return ErrOk;
}


void fsm_msgSkycoinSignMessageImpl(SkycoinSignMessage* msg,
								   ResponseSkycoinSignMessage *resp)
{
	if (storage_hasMnemonic() == false) {
		fsm_sendFailure(FailureType_Failure_AddressGeneration, "Mnemonic not set");
		return;
	}
	CHECK_PIN_UNCACHED
	uint8_t pubkey[33] = {0};
	uint8_t seckey[32] = {0};
	fsm_getKeyPairAtIndex(1, pubkey, seckey, NULL, msg->address_n);
	uint8_t digest[32] = {0};
	if (is_digest(msg->message) == false) {
		compute_sha256sum((const uint8_t *)msg->message, digest, strlen(msg->message));
	} else {
		writebuf_fromhexstr(msg->message, digest);
	}
	uint8_t signature[65];
	int res = ecdsa_skycoin_sign(rand(), seckey, digest, signature);
	if (res == 0) {
		layoutRawMessage("Signature success");
	} else {
		layoutRawMessage("Signature failed");
	}
	const size_t hex_len = 2 * sizeof(signature);
	char signature_in_hex[hex_len];
	tohex(signature_in_hex, signature, sizeof(signature));
	memcpy(resp->signed_message, signature_in_hex, hex_len);
	msg_write(MessageType_MessageType_ResponseSkycoinSignMessage, resp);
	layoutHome();
}