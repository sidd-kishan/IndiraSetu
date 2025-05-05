#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_TX_SIZE 8192
#define MAX_PATTERN 256

void print_help(const char *prog) {
    printf("Usage:\n");
    printf("  %s \"<message>\" <tx_length> [--prefix \"0xAA,...\"] [--suffix \"0xBB,...\"] [--pattern \"0x00,0xFF,...\"]\n", prog);
    printf("Arguments:\n");
    printf("  \"<message>\"   Input string to convert bitwise into TxBuffer\n");
    printf("  <tx_length>    Desired TxBuffer length in bytes (must be multiple of 8)\n");
    printf("  --prefix       (optional) Hex bytes to insert before each full message+CRC cycle\n");
    printf("  --suffix       (optional) Hex bytes to insert after each full message+CRC cycle\n");
    printf("  --pattern      (optional) Pattern to search in TxBuffer\n");
    exit(1);
}

unsigned char crc8(const unsigned char *data, int len) {
    unsigned char crc = 0x00;
    for (int i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x07;
            else
                crc <<= 1;
        }
    }
    return crc;
}

int parse_pattern(const char *pattern_str, unsigned char *buffer, int max_len) {
    int count = 0;
    char temp[MAX_PATTERN];
    strncpy(temp, pattern_str, MAX_PATTERN - 1);
    temp[MAX_PATTERN - 1] = '\0';

    char *token = strtok(temp, ",");
    while (token && count < max_len) {
        while (isspace(*token)) token++;
        if (strncmp(token, "0x", 2) == 0 || strncmp(token, "0X", 2) == 0)
            buffer[count++] = (unsigned char)strtol(token, NULL, 16);
        else
            buffer[count++] = (unsigned char)atoi(token);
        token = strtok(NULL, ",");
    }
    return count;
}

void bit_expand_byte(unsigned char byte, unsigned char *out) {
    for (int i = 0; i < 8; i++) {
        out[i] = (byte & (1 << (7 - i))) ? 0xFF : 0x00;
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3 || strcmp(argv[1], "--help") == 0) {
        print_help(argv[0]);
    }

    const char *input = argv[1];
    int tx_len = atoi(argv[2]);
    int input_len = strlen(input);

    if (tx_len <= 0 || tx_len > MAX_TX_SIZE || tx_len % 8 != 0) {
        fprintf(stderr, "Error: tx_length must be a positive multiple of 8 and <= %d\n", MAX_TX_SIZE);
        return 1;
    }

    unsigned char prefix[MAX_PATTERN], suffix[MAX_PATTERN], pattern[MAX_PATTERN];
    int prefix_len = 0, suffix_len = 0, pattern_len = 0;

    for (int i = 3; i < argc - 1; i++) {
        if (strcmp(argv[i], "--prefix") == 0)
            prefix_len = parse_pattern(argv[i + 1], prefix, MAX_PATTERN);
        else if (strcmp(argv[i], "--suffix") == 0)
            suffix_len = parse_pattern(argv[i + 1], suffix, MAX_PATTERN);
        else if (strcmp(argv[i], "--pattern") == 0)
            pattern_len = parse_pattern(argv[i + 1], pattern, MAX_PATTERN);
    }

    unsigned char TxBuffer[MAX_TX_SIZE];
    int buffer_index = 0;
    int char_idx = 0;
    int repetition_count = 0;

    printf("TxBuffer[] = {\n");

    while (buffer_index < tx_len) {
        int space_left = tx_len - buffer_index;
        int full_block_size = prefix_len + (input_len * 8) + 8 + suffix_len;

        if (space_left >= full_block_size) {
            // Full cycle: prefix + message + CRC + suffix
            if (prefix_len > 0) {
                for (int i = 0; i < prefix_len; i++) {
                    TxBuffer[buffer_index++] = prefix[i];
                    printf("  0x%02X", prefix[i]);
                    if (i < prefix_len - 1 || input_len > 0) printf(", ");
                }
                printf("  // Prefix\n");
            }

            for (int i = 0; i < input_len; i++) {
                unsigned char expanded[8];
                bit_expand_byte(input[i], expanded);
                for (int b = 0; b < 8; b++) {
                    TxBuffer[buffer_index++] = expanded[b];
                    printf("  0x%02X", expanded[b]);
                    if (b < 7 || (i < input_len - 1) || suffix_len > 0 || 1) printf(", ");
                }
                printf("  // '%c'\n", isprint(input[i]) ? input[i] : '.');
            }

            // CRC8 for current message
            unsigned char crc = crc8((const unsigned char *)input, input_len);
            unsigned char crc_exp[8];
            bit_expand_byte(crc, crc_exp);
            for (int b = 0; b < 8; b++) {
                TxBuffer[buffer_index++] = crc_exp[b];
                printf("  0x%02X", crc_exp[b]);
                if (b < 7 || suffix_len > 0) printf(", ");
            }
            printf("  // CRC8: 0x%02X\n", crc);

            if (suffix_len > 0) {
                for (int i = 0; i < suffix_len; i++) {
                    TxBuffer[buffer_index++] = suffix[i];
                    printf("  0x%02X", suffix[i]);
                    if (i < suffix_len - 1 || buffer_index < tx_len) printf(", ");
                }
                printf("  // Suffix\n");
            }

            repetition_count++;
        } else {
            // Not enough room for full repetition, just fill remaining with bit-expanded message chars
            while (buffer_index + 8 <= tx_len) {
                unsigned char c = input[char_idx++];
                if (char_idx >= input_len) char_idx = 0;

                unsigned char bits[8];
                bit_expand_byte(c, bits);
                for (int i = 0; i < 8; i++) {
                    TxBuffer[buffer_index++] = bits[i];
                    printf("  0x%02X", bits[i]);
                    if (buffer_index < tx_len) printf(", ");
                }
                printf("  // '%c'\n", isprint(c) ? c : '.');
            }
        }
    }

    printf("};\n");

    // Pattern Search
    if (pattern_len > 0) {
        printf("\nSearching for pattern: ");
        for (int i = 0; i < pattern_len; i++) {
            printf("0x%02X", pattern[i]);
            if (i < pattern_len - 1) printf(", ");
        }
        printf("\n");

        int found = 0;
        for (int i = 0; i <= tx_len - pattern_len; i++) {
            int match = 1;
            for (int j = 0; j < pattern_len; j++) {
                if (TxBuffer[i + j] != pattern[j]) {
                    match = 0;
                    break;
                }
            }
            if (match) {
                printf("Pattern found at TxBuffer index: %d (byte #%d)\n", i, (i / 8) + 1);
                found = 1;
            }
        }
        if (!found)
            printf("Pattern not found.\n");
    }

    return 0;
}
