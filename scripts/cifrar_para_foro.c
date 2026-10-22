/* Genera el reto que publicamos en el foro de Canvas: cifra un texto propio
 * con una llave DES (que NO se revela) y muestra el resultado en base64,
 * listo para pegar junto con la frase objetivo.
 *
 * Uso: cifrar_para_foro <llave_decimal> "<texto con la frase objetivo>"
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define OPENSSL_SUPPRESS_DEPRECATED
#include <openssl/des.h>
#include <openssl/evp.h>

#define DES_BLOCK_SIZE 8U

static void logical_key_to_des(uint64_t key, unsigned char des_key[8]) {
    for (size_t byte = 0; byte < 8; ++byte) {
        const unsigned int shift = (unsigned int)((7U - byte) * 7U);
        des_key[byte] = (unsigned char)(((key >> shift) & UINT64_C(0x7f)) << 1);
    }
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <llave_decimal> <texto>\n", argv[0]);
        return 1;
    }

    const uint64_t key = strtoull(argv[1], NULL, 10);
    const char *text = argv[2];
    const size_t text_len = strlen(text);
    const size_t padding = DES_BLOCK_SIZE - (text_len % DES_BLOCK_SIZE);
    const size_t padded_len = text_len + padding;

    unsigned char *buffer = malloc(padded_len);
    memcpy(buffer, text, text_len);
    memset(buffer + text_len, (unsigned char)padding, padding);

    unsigned char des_key[8];
    logical_key_to_des(key, des_key);

    DES_cblock key_block;
    DES_key_schedule schedule;
    memcpy(key_block, des_key, sizeof(key_block));
    DES_set_odd_parity(&key_block);
    DES_set_key_unchecked(&key_block, &schedule);
    for (size_t offset = 0; offset < padded_len; offset += DES_BLOCK_SIZE) {
        DES_cblock in, out;
        memcpy(in, buffer + offset, sizeof(in));
        DES_ecb_encrypt(&in, &out, &schedule, DES_ENCRYPT);
        memcpy(buffer + offset, out, sizeof(out));
    }

    FILE *f = fopen("datos/cifrado_para_foro.bin", "wb");
    fwrite(buffer, 1, padded_len, f);
    fclose(f);

    unsigned char *b64 = malloc(((padded_len + 2) / 3) * 4 + 1);
    const int b64_len = EVP_EncodeBlock(b64, buffer, (int)padded_len);
    b64[b64_len] = '\0';

    fprintf(stderr, "Bytes cifrados: %zu (guardado en datos/cifrado_para_foro.bin)\n", padded_len);
    fprintf(stderr, "Llave (NO publicar): %llu\n\n", (unsigned long long)key);
    printf("%s\n", b64);
    return 0;
}
