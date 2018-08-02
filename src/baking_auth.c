#include "apdu.h"
#include "baking_auth.h"
#include "keys.h"
#include "protocol.h"
#include "ui_prompt.h"
#include "to_string.h"

#include "os.h"
#include "cx.h"

#include <string.h>

WIDE nvram_data N_data_real;

bool is_valid_level(level_t lvl) {
    return !(lvl & 0xC0000000);
}

void write_highest_level(level_t lvl, bool is_endorsement) {
    if (!is_valid_level(lvl)) return;
    nvram_data new_data;
    memcpy(&new_data, &N_data, sizeof(new_data));
    new_data.highest_level = lvl;
    new_data.had_endorsement = is_endorsement;
    nvm_write((void*)&N_data, &new_data, sizeof(N_data));
    change_idle_display(N_data.highest_level);
}

void authorize_baking(cx_curve_t curve, uint32_t *bip32_path, uint8_t path_length) {
    if (bip32_path == NULL || path_length > MAX_BIP32_PATH || path_length == 0) {
        return;
    }

    nvram_data new_baking_details;
    new_baking_details.highest_level = N_data.highest_level;
    new_baking_details.curve = curve;
    memcpy(new_baking_details.bip32_path, bip32_path, path_length * sizeof(*bip32_path));
    new_baking_details.path_length = path_length;
    nvm_write((void*)&N_data, &new_baking_details, sizeof(N_data));
    change_idle_display(N_data.highest_level);
}

bool is_level_authorized(level_t level, bool is_endorsement) {
    if (!is_valid_level(level)) return false;
    if (level > N_data.highest_level) return true;

    // Levels are tied. In order for this to be OK, this must be an endorsement, and we must not
    // have previously seen an endorsement.
    return is_endorsement && !N_data.had_endorsement;
}

bool is_path_authorized(cx_curve_t curve, uint32_t *bip32_path, uint8_t path_length) {
    return bip32_path != NULL &&
        path_length != 0 &&
        path_length == N_data.path_length &&
        curve == N_data.curve &&
        memcmp(bip32_path, N_data.bip32_path, path_length * sizeof(*bip32_path)) == 0;
}

void guard_baking_authorized(cx_curve_t curve, void *data, int datalen, uint32_t *bip32_path,
                             uint8_t path_length) {
    if (!is_path_authorized(curve, bip32_path, path_length)) THROW(EXC_SECURITY);

    struct parsed_baking_data baking_info;
    if (!parse_baking_data(data, datalen, &baking_info)) THROW(EXC_SECURITY);

    if (!is_level_authorized(baking_info.level, baking_info.is_endorsement)) THROW(EXC_SECURITY);
}

void update_high_water_mark(void *data, int datalen) {
    struct parsed_baking_data baking_info;
    if (!parse_baking_data(data, datalen, &baking_info)) {
        return; // Must be signing a delegation
    }

    write_highest_level(baking_info.level, baking_info.is_endorsement);
}

void update_auth_text(void) {
    if (N_data.path_length == 0) {
        strcpy(baking_auth_text, "No Key Authorized");
    } else {
        cx_ecfp_public_key_t pub_key;
        cx_ecfp_private_key_t priv_key;
        generate_key_pair(N_data.curve, N_data.path_length, N_data.bip32_path,
                          &pub_key, &priv_key);
        os_memset(&priv_key, 0, sizeof(priv_key));
        pubkey_to_pkh_string(baking_auth_text, sizeof(baking_auth_text), N_data.curve, &pub_key);
    }
}

static char address_display_data[64]; // Should be more than big enough

static const char *const pubkey_labels[] = {
    "Provide",
    "Public Key",
    NULL,
};

static const char *const pubkey_values[] = {
    "Public Key?",
    address_display_data,
    NULL,
};

#ifdef BAKING_APP
static const char *const baking_labels[] = {
    "Authorize baking",
    "Public Key",
    NULL,
};

static const char *const baking_values[] = {
    "With Public Key?",
    address_display_data,
    NULL,
};

// TODO: Unshare code with next function
void prompt_contract_for_baking(struct parsed_contract *contract, callback_t ok_cb, callback_t cxl_cb) {
    if (!parsed_contract_to_string(address_display_data, sizeof(address_display_data), contract)) {
        THROW(EXC_WRONG_VALUES);
    }

    ui_prompt_multiple(baking_labels, baking_values, ok_cb, cxl_cb);
}
#endif

void prompt_address(
#ifndef BAKING_APP
    __attribute__((unused))
#endif
    bool baking,
                    cx_curve_t curve, const cx_ecfp_public_key_t *key, callback_t ok_cb,
                    callback_t cxl_cb) {
    if (!pubkey_to_pkh_string(address_display_data, sizeof(address_display_data), curve, key)) {
        THROW(EXC_WRONG_VALUES);
    }

#ifdef BAKING_APP
    if (baking) {
        ui_prompt_multiple(baking_labels, baking_values, ok_cb, cxl_cb);
    } else {
#endif
        ui_prompt_multiple(pubkey_labels, pubkey_values, ok_cb, cxl_cb);
#ifdef BAKING_APP
    }
#endif
}

struct __attribute__((packed)) block {
    char magic_byte;
    uint32_t chain_id;
    level_t level;
    uint8_t proto;
    // ... beyond this we don't care
};

struct __attribute__((packed)) endorsement {
    uint8_t magic_byte;
    uint32_t chain_id;
    uint8_t branch[32];
    uint8_t tag;
    uint32_t level;
};

bool parse_baking_data(const void *data, size_t length, struct parsed_baking_data *out) {
    switch (get_magic_byte(data, length)) {
        case MAGIC_BYTE_BAKING_OP:
            if (length != sizeof(struct endorsement)) return false;
            const struct endorsement *endorsement = data;
            // TODO: Check chain ID
            out->is_endorsement = true;
            out->level = READ_UNALIGNED_BIG_ENDIAN(level_t, &endorsement->level);
            return true;
        case MAGIC_BYTE_BLOCK:
            if (length < sizeof(struct block)) return false;
            // TODO: Check chain ID
            out->is_endorsement = false;
            const struct block *block = data;
            out->level = READ_UNALIGNED_BIG_ENDIAN(level_t, &block->level);
            return true;
        case MAGIC_BYTE_INVALID:
        default:
            return false;
    }
}
