#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_TX_SIZE 8192

void print_usage(const char *prog) {
    printf("Usage: %s \"message\" tx_len [--pattern \"0x00,0xFF,...\"]\n", prog);
    printf("  message:  input message in quotes\n");
    printf("  tx_len:   desired length of TxBuffer in bytes (must be multiple of 8)\n");
    printf("  --pattern: (optional) comma-separated pattern to search (e.g., \"0x00,0xFF\")\n");
    exit(1);
}

int parse_pattern(const char *pattern_str, unsigned char *pattern_bytes) {
    int count = 0;
    const char *ptr = pattern_str;
    while (*ptr) {
        if (sscanf(ptr, "0x%hhx", &pattern_bytes[count]) == 1) {
            count++;
            ptr = strchr(ptr, ',');
            if (!ptr) break;
            ptr++; // move past comma
        } else {
            break;
        }
    }
    return count;
}

int main(int argc, char *argv[]) {
    if (argc < 3) print_usage(argv[0]);

    const char *input = argv[1];
    int tx_len = atoi(argv[2]);
    if (tx_len <= 0 || tx_len > MAX_TX_SIZE || tx_len % 8 != 0) {
        fprintf(stderr, "Error: tx_len must be a positive multiple of 8 and <= %d\n", MAX_TX_SIZE);
        return 1;
    }

    // Parse optional pattern
    unsigned char search_pattern[64];
    int search_pattern_len = 0;
    if (argc >= 5 && strcmp(argv[3], "--pattern") == 0) {
        search_pattern_len = parse_pattern(argv[4], search_pattern);
        if (search_pattern_len == 0) {
            fprintf(stderr, "Invalid pattern format.\n");
            return 1;
        }
    }

    int input_len = strlen(input);
    int total_bytes_needed = tx_len;
    int bits_per_char = 8;
    int buffer_index = 0;

    unsigned char TxBuffer[MAX_TX_SIZE];

    if (input_len * 8 >= tx_len) {
        printf("Input is sufficient to fill TxBuffer without repetition.\n\n");
    } else {
        printf("Input will be repeated to fill TxBuffer.\n\n");
    }

    printf("TxBuffer[] = {\n");

    int char_idx = 0;
    int byte_count = 0;

    while (buffer_index < tx_len) {
        // Insert 8 bytes for one character
        unsigned char c = input[char_idx++];
        if (char_idx >= input_len) char_idx = 0;

        for (int bit = 7; bit >= 0; bit--) {
            unsigned char bit_val = (c >> bit) & 1;
            TxBuffer[buffer_index++] = bit_val ? 0xFF : 0x00;
        }

        printf("  ");
        for (int i = 0; i < 8; i++) {
            printf("0x%02X", TxBuffer[buffer_index - 8 + i]);
            if (i < 7) printf(", ");
        }

        printf("  // byte #%d (", ++byte_count);
        if (isprint(c)) {
            printf("'%c')\n", c);
        } else {
            printf("0x%02X)\n", c);
        }
    }

    printf("};\n");

    // Pattern search
    if (search_pattern_len > 0) {
        printf("\nSearching for pattern: ");
        for (int i = 0; i < search_pattern_len; i++) {
            printf("0x%02X", search_pattern[i]);
            if (i < search_pattern_len - 1) printf(", ");
        }
        printf("\n");

        int found = 0;
        for (int i = 0; i <= tx_len - search_pattern_len; i++) {
            int match = 1;
            for (int j = 0; j < search_pattern_len; j++) {
                if (TxBuffer[i + j] != search_pattern[j]) {
                    match = 0;
                    break;
                }
            }
            if (match) {
                printf("Pattern found at TxBuffer index: %d (byte #%d)\n", i, (i / 8) + 1);
                found = 1;
            }
        }

        if (!found) {
            printf("Pattern not found.\n");
        }
    }

    return 0;
}
