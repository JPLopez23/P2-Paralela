/*
 * Demostracion academica de fuerza bruta DES con MPI.
 * DES y ECB son obsoletos e inseguros para sistemas reales; se conservan
 * exclusivamente porque forman parte de la consigna del curso.
 */
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef BRUTEFORCE_CORE_TEST
#include <mpi.h>
/* DES es intencional por la consigna; OpenSSL 3 lo marca como obsoleto. */
#define OPENSSL_SUPPRESS_DEPRECATED
#include <openssl/des.h>
#endif

#define DES_BLOCK_SIZE 8U
#define DES_KEYSPACE_SIZE (UINT64_C(1) << 56)
#define MAX_TEXT_BYTES (1024U * 1024U)
#define SEARCH_BATCH_SIZE UINT64_C(4096)

typedef struct {
    const char *text;
    const char *phrase;
    uint64_t key;
    uint64_t upper;
} CliOptions;

/* Calcula el rango de claves que corresponde a cada proceso MPI. */
bool compute_range(uint64_t key_count, int process_count, int rank,
                   uint64_t *lower, uint64_t *upper) {
    if (process_count <= 0 || rank < 0 || rank >= process_count ||
        lower == NULL || upper == NULL) {
        return false;
    }

    const uint64_t processes = (uint64_t)process_count;
    const uint64_t base = key_count / processes;
    const uint64_t remainder = key_count % processes;
    const uint64_t rank_u = (uint64_t)rank;
    const uint64_t extra_before = rank_u < remainder ? rank_u : remainder;
    const uint64_t local_size = base + (rank_u < remainder ? 1U : 0U);

    *lower = rank_u * base + extra_before;
    *upper = *lower + local_size;
    return true;
}

/* Calcula el tamaño necesario para completar un bloque DES. */
bool des_padded_length(size_t input_len, size_t *output_len) {
    if (output_len == NULL) {
        return false;
    }

    const size_t padding = DES_BLOCK_SIZE - (input_len % DES_BLOCK_SIZE);
    if (input_len > SIZE_MAX - padding) {
        return false;
    }
    *output_len = input_len + padding;
    return true;
}

/* Copia el texto y agrega el relleno PKCS#7 requerido por DES. */
bool pad_plaintext(const unsigned char *input, size_t input_len,
                   unsigned char **output, size_t *output_len) {
    size_t padded_len = 0;
    if (output == NULL || output_len == NULL ||
        (input == NULL && input_len != 0) ||
        !des_padded_length(input_len, &padded_len)) {
        return false;
    }

    unsigned char *buffer = malloc(padded_len);
    if (buffer == NULL) {
        return false;
    }
    if (input_len != 0) {
        memcpy(buffer, input, input_len);
    }
    const unsigned char padding = (unsigned char)(padded_len - input_len);
    memset(buffer + input_len, padding, padded_len - input_len);
    *output = buffer;
    *output_len = padded_len;
    return true;
}

/* Valida el relleno y obtiene el tamaño original del texto. */
bool remove_pkcs7_padding(const unsigned char *buffer, size_t buffer_len,
                          size_t *plain_len) {
    if (buffer == NULL || plain_len == NULL || buffer_len == 0 ||
        buffer_len % DES_BLOCK_SIZE != 0) {
        return false;
    }

    const unsigned char padding = buffer[buffer_len - 1];
    if (padding == 0 || padding > DES_BLOCK_SIZE || padding > buffer_len) {
        return false;
    }

    unsigned char mismatch = 0;
    for (size_t i = 0; i < padding; ++i) {
        mismatch |= (unsigned char)(buffer[buffer_len - 1 - i] ^ padding);
    }
    if (mismatch != 0) {
        return false;
    }
    *plain_len = buffer_len - padding;
    return true;
}

/* Comprueba que el texto descifrado sea exactamente el esperado. */
bool plaintext_matches_expected(const unsigned char *recovered,
                                size_t recovered_len,
                                const char *expected) {
    if (recovered == NULL || expected == NULL) {
        return false;
    }
    const size_t expected_len = strlen(expected);
    return recovered_len == expected_len &&
           memcmp(recovered, expected, recovered_len) == 0;
}

static void logical_key_to_des(uint64_t key, unsigned char des_key[8]) {
    /* Convierte la clave lógica al formato de ocho bytes usado por DES. */
    for (size_t byte = 0; byte < 8; ++byte) {
        const unsigned int shift = (unsigned int)((7U - byte) * 7U);
        des_key[byte] = (unsigned char)(((key >> shift) & UINT64_C(0x7f)) << 1);
    }
}

static bool crypt_buffer(uint64_t key, unsigned char *buffer, size_t len,
                         bool decrypting) {
    if (buffer == NULL || len == 0 || len % DES_BLOCK_SIZE != 0 ||
        key >= DES_KEYSPACE_SIZE) {
        return false;
    }

    unsigned char des_key[8];
    logical_key_to_des(key, des_key);

#ifdef BRUTEFORCE_CORE_TEST
    /* Usa una transformación determinista para probar el núcleo sin MPI ni DES. */
    (void)decrypting;
    for (size_t i = 0; i < len; ++i) {
        const unsigned int rotation = (unsigned int)((i % 8U) * 7U);
        const unsigned char stream =
            (unsigned char)(((key >> rotation) & UINT64_C(0xff)) ^
                            (UINT64_C(0x5b) + i * UINT64_C(29)));
        buffer[i] ^= stream;
    }
    return true;
#else
    /* Procesa el contenido por bloques de ocho bytes con DES en modo ECB. */
    DES_cblock key_block;
    DES_key_schedule schedule;
    memcpy(key_block, des_key, sizeof(key_block));
    DES_set_odd_parity(&key_block);
    DES_set_key_unchecked(&key_block, &schedule);
    for (size_t offset = 0; offset < len; offset += DES_BLOCK_SIZE) {
        DES_cblock input_block;
        DES_cblock output_block;
        memcpy(input_block, buffer + offset, sizeof(input_block));
        DES_ecb_encrypt(&input_block, &output_block, &schedule,
                        decrypting ? DES_DECRYPT : DES_ENCRYPT);
        memcpy(buffer + offset, output_block, sizeof(output_block));
    }
    return true;
#endif
}

bool encrypt_buffer(uint64_t key, unsigned char *buffer, size_t len) {
    return crypt_buffer(key, buffer, len, false);
}

bool decrypt_buffer(uint64_t key, unsigned char *buffer, size_t len) {
    return crypt_buffer(key, buffer, len, true);
}

/* Guarda un mensaje de error en el búfer proporcionado por el llamador. */
static void set_error(char *error, size_t error_size, const char *message) {
    if (error != NULL && error_size != 0) {
        (void)snprintf(error, error_size, "%s", message);
    }
}

/* Convierte una cadena decimal en una clave válida de 56 bits. */
static bool parse_uint56(const char *value, uint64_t *parsed) {
    if (value == NULL || value[0] == '\0' || value[0] == '-' || parsed == NULL) {
        return false;
    }
    errno = 0;
    char *end = NULL;
    const unsigned long long number = strtoull(value, &end, 10);
    if (errno == ERANGE || end == value || *end != '\0' ||
        number > DES_KEYSPACE_SIZE) {
        return false;
    }
    *parsed = (uint64_t)number;
    return true;
}

