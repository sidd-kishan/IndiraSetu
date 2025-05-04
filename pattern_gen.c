#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_PATTERN 256

void print_help(const char *program_name) {
    printf("Usage:\n");
    printf("  %s \"<message>\" <tx_length> [--prefix \"<hex,hex,...>\"] [--suffix \"<hex,hex,...>\"]\n", program_name);
    printf("\nArguments:\n");
    printf("  \"<message>\"   The input string to convert bitwise into TxBuffer\n");
    printf("  <tx_length>    Desired TxBuffer length in bytes (must be multiple of 8)\n");
    printf("  --prefix       (optional) Comma-separated hex values to prepend per cycle\n");
    printf("  --suffix       (optional) Comma-separated hex values to append per cycle\n");
    printf("\nExample:\n");
    printf("  %s \"Hi\" 48 --prefix \"0xAA\" --suffix \"0xBB\"\n", program_name);
    printf("  %s --help\n", program_name);
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

int main(int argc, char *argv[]) {
    if (argc < 3 || strcmp(argv[1], "--help") == 0) {
        print_help(argv[0]);
        return 0;
    }

    const char *input = argv[1];
    int desired_tx_len = atoi(argv[2]);

    if (desired_tx_len <= 0 || desired_tx_len % 8 != 0) {
        fprintf(stderr, "Error: <tx_length> must be a positive multiple of 8.\n");
        return 1;
    }

    unsigned char prefix[MAX_PATTERN], suffix[MAX_PATTERN];
    int prefix_len = 0, suffix_len = 0;

    for (int i = 3; i < argc - 1; i++) {
        if (strcmp(argv[i], "--prefix") == 0) {
            prefix_len = parse_pattern(argv[i + 1], prefix, MAX_PATTERN);
        } else if (strcmp(argv[i], "--suffix") == 0) {
            suffix_len = parse_pattern(argv[i + 1], suffix, MAX_PATTERN);
        }
    }

    size_t input_len = strlen(input);
    size_t group_payload_len = prefix_len + 8 * input_len + suffix_len;

    if (group_payload_len == 0 || (group_payload_len > desired_tx_len)) {
        fprintf(stderr, "Error: Cannot fit one full cycle (prefix + message + suffix) in the tx_length.\n");
        return 1;
    }

    int max_groups = desired_tx_len / group_payload_len;
    size_t actual_len = max_groups * group_payload_len;

    if ((input_len * 8) >= desired_tx_len) {
        printf("Input is sufficient to fill TxBuffer without repetition.\n");
    } else {
        printf("Repeating pattern (prefix + message + suffix) to fill buffer.\n");
    }

    unsigned char *TxBuffer = malloc(actual_len);
    if (!TxBuffer) {
        fprintf(stderr, "Memory allocation failed.\n");
        return 1;
    }

    int tx_index = 0;
    int byte_num = 1;

    printf("\nTxBuffer[] = {\n");

    for (int group = 0; group < max_groups; group++) {
        // Prefix
        for (int i = 0; i < prefix_len && tx_index < actual_len; i++) {
            TxBuffer[tx_index++] = prefix[i];
            printf("0x%02X", prefix[i]);
            if (tx_index < actual_len) printf(", ");
        }

        // Message (bitwise)
        for (size_t j = 0; j < input_len && tx_index + 8 <= actual_len; j++) {
            unsigned char c = input[j];
            for (int bit = 7; bit >= 0; bit--) {
                unsigned char val = ((c >> bit) & 1) ? 0xFF : 0x00;
                TxBuffer[tx_index++] = val;
                printf("0x%02X", val);
                if (bit > 0 || (j != input_len - 1) || suffix_len > 0) printf(", ");
            }

            // Comment per byte
            printf(" // byte #%d ", byte_num++);
            if (isprint(c))
                printf("('%c')", c);
            else
                printf("(0x%02X)", c);
            printf("\n");
        }

        // Suffix
        for (int i = 0; i < suffix_len && tx_index < actual_len; i++) {
            TxBuffer[tx_index++] = suffix[i];
            printf("0x%02X", suffix[i]);
            if (i != suffix_len - 1 || group != max_groups - 1) printf(", ");
        }

        if (suffix_len > 0) printf("\n");
    }

    printf("};\n");
    free(TxBuffer);
    return 0;
}
