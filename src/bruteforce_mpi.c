/*
 * Demostracion academica de fuerza bruta DES con MPI.
 * DES y ECB son obsoletos e inseguros para sistemas reales; se conservan
 * exclusivamente porque forman parte de la consigna del curso.
 */
/* Necesario para que unistd.h declare gethostname() con -std=c11. */
#define _POSIX_C_SOURCE 200809L
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
#include <unistd.h>
/* DES es intencional por la consigna; OpenSSL 3 lo marca como obsoleto. */
#define OPENSSL_SUPPRESS_DEPRECATED
#include <openssl/des.h>
#include <openssl/sha.h>
#include <openssl/md5.h>
#endif

#define DES_BLOCK_SIZE 8U
#define DES_KEYSPACE_SIZE (UINT64_C(1) << 56)
#define MAX_TEXT_BYTES (1024U * 1024U)
#define SEARCH_BATCH_SIZE UINT64_C(4096)

/* Variantes de interoperabilidad: 0=7-bit BE, 1=7-bit LE,
 * 2=entero de 64 bits LE, 3=entero de 64 bits BE, 4=mascaras int32 heredadas,
 * 5=decimal con ceros, 6=decimal con NUL, 7=hexadecimal con ceros,
 * 8/9=grupos de 7 bits BE/LE sin desplazamiento, 10=SHA256(decimal),
 * 11=MD5(decimal), 12=DES_string_to_key(decimal),
 * 13/14=decimal con espacios a derecha/izquierda.
 * Se seleccionan al compilar, sin alterar el formato predeterminado. */
#ifndef DES_KEY_LAYOUT
#define DES_KEY_LAYOUT 0
#endif

typedef struct {
    const char *text;
    const char *phrase;
    const char *cipher_file;
    uint64_t key;
    uint64_t lower;
    uint64_t upper;
    bool has_key;
    bool has_lower;
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
#if DES_KEY_LAYOUT >= 10 && DES_KEY_LAYOUT <= 14
    char encoded[32] = {0};
    snprintf(encoded, sizeof(encoded), "%llu", (unsigned long long)key);
#if DES_KEY_LAYOUT == 10
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256((const unsigned char *)encoded, strlen(encoded), digest);
    memcpy(des_key, digest, 8);
#elif DES_KEY_LAYOUT == 11
    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5((const unsigned char *)encoded, strlen(encoded), digest);
    memcpy(des_key, digest, 8);
#elif DES_KEY_LAYOUT == 12
    DES_string_to_key(encoded, (DES_cblock *)des_key);
#elif DES_KEY_LAYOUT == 13
    memset(des_key, ' ', 8);
    memcpy(des_key, encoded, strlen(encoded) < 8 ? strlen(encoded) : 8);
#else
    snprintf(encoded, sizeof(encoded), "%8llu", (unsigned long long)key);
    memcpy(des_key, encoded, 8);
#endif
#elif DES_KEY_LAYOUT == 4
    /* Emula las mascaras int32 del ejemplo original sin desplazamientos UB. */
    uint64_t packed = 0;
    for (unsigned int i = 0; i < 8; ++i) {
        key <<= 1;
        const uint32_t mask = UINT32_C(0xfe) << ((i * 8U) % 32U);
        const uint64_t extended = (uint64_t)(int64_t)(int32_t)mask;
        packed += key & extended;
    }
    for (size_t i = 0; i < 8; ++i) des_key[i] = (unsigned char)(packed >> (i * 8U));
#elif DES_KEY_LAYOUT >= 5 && DES_KEY_LAYOUT <= 7
    char encoded[32] = {0};
#if DES_KEY_LAYOUT == 5
    snprintf(encoded, sizeof(encoded), "%08llu", (unsigned long long)key);
#elif DES_KEY_LAYOUT == 6
    snprintf(encoded, sizeof(encoded), "%llu", (unsigned long long)key);
#else
    snprintf(encoded, sizeof(encoded), "%08llx", (unsigned long long)key);
#endif
    memcpy(des_key, encoded, 8);
#else
    for (size_t byte = 0; byte < 8; ++byte) {
#if DES_KEY_LAYOUT == 1
        des_key[byte] = (unsigned char)(((key >> (byte * 7U)) & UINT64_C(0x7f)) << 1);
#elif DES_KEY_LAYOUT == 2
        des_key[byte] = (unsigned char)(key >> (byte * 8U));
#elif DES_KEY_LAYOUT == 3
        des_key[byte] = (unsigned char)(key >> ((7U - byte) * 8U));
#elif DES_KEY_LAYOUT == 8
        des_key[byte] = (unsigned char)((key >> ((7U - byte) * 7U)) & 0x7f);
#elif DES_KEY_LAYOUT == 9
        des_key[byte] = (unsigned char)((key >> (byte * 7U)) & 0x7f);
#elif DES_KEY_LAYOUT == 0
        const unsigned int shift = (unsigned int)((7U - byte) * 7U);
        des_key[byte] = (unsigned char)(((key >> shift) & UINT64_C(0x7f)) << 1);
#else
#error "DES_KEY_LAYOUT invalido"
#endif
    }
#endif
}

static bool crypt_buffer(uint64_t key, unsigned char *buffer, size_t len,
                         bool decrypting) {
    if (buffer == NULL || len == 0 || len % DES_BLOCK_SIZE != 0 ||
        key >= DES_KEYSPACE_SIZE) {
        return false;
    }

    unsigned char des_key[8];
    logical_key_to_des(key, des_key);
#ifdef DES_RPC_PARITY
    /* des_setparity de SunRPC descarta el bit alto antes de ajustar paridad. */
    for (size_t i = 0; i < 8; ++i) des_key[i] &= 0x7f;
#endif

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
#ifdef DES_DIAGNOSTIC_CBC
    DES_cblock iv = {0};
    DES_ncbc_encrypt(buffer, buffer, (long)len, &schedule, &iv,
                     decrypting ? DES_DECRYPT : DES_ENCRYPT);
#else
    for (size_t offset = 0; offset < len; offset += DES_BLOCK_SIZE) {
        DES_cblock input_block;
        DES_cblock output_block;
        memcpy(input_block, buffer + offset, sizeof(input_block));
        DES_ecb_encrypt(&input_block, &output_block, &schedule,
                        decrypting ? DES_DECRYPT : DES_ENCRYPT);
        memcpy(buffer + offset, output_block, sizeof(output_block));
    }
#endif
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

/* Lee y valida las opciones recibidas desde la línea de comandos.
 * Admite dos modos: autoprueba (--text + --key, cifra y luego busca) o
 * ataque real (--cipher-file, descifra un binario de origen desconocido). */
bool parse_cli_options(int argc, char **argv, CliOptions *options,
                       char *error, size_t error_size) {
    if (options == NULL || argv == NULL) {
        set_error(error, error_size, "Parametros internos invalidos.");
        return false;
    }

    CliOptions parsed = {0};
    bool have_text = false;
    bool have_phrase = false;
    bool have_key = false;
    bool have_upper = false;
    bool have_cipher_file = false;
    bool have_lower = false;

    for (int i = 1; i < argc; i += 2) {
        if (i + 1 >= argc) {
            set_error(error, error_size, "Cada opcion requiere un valor.");
            return false;
        }
        const char *name = argv[i];
        const char *value = argv[i + 1];
        if (strcmp(name, "--text") == 0 && !have_text) {
            parsed.text = value;
            have_text = true;
        } else if (strcmp(name, "--phrase") == 0 && !have_phrase) {
            parsed.phrase = value;
            have_phrase = true;
        } else if (strcmp(name, "--cipher-file") == 0 && !have_cipher_file) {
            parsed.cipher_file = value;
            have_cipher_file = true;
        } else if (strcmp(name, "--key") == 0 && !have_key) {
            if (!parse_uint56(value, &parsed.key)) {
                set_error(error, error_size, "--key debe ser un entero DES valido.");
                return false;
            }
            have_key = true;
            parsed.has_key = true;
        } else if (strcmp(name, "--lower") == 0 && !have_lower) {
            if (!parse_uint56(value, &parsed.lower)) {
                set_error(error, error_size, "--lower debe ser un entero DES valido.");
                return false;
            }
            have_lower = true;
            parsed.has_lower = true;
        } else if (strcmp(name, "--upper") == 0 && !have_upper) {
            if (!parse_uint56(value, &parsed.upper)) {
                set_error(error, error_size, "--upper debe ser un entero DES valido.");
                return false;
            }
            have_upper = true;
        } else {
            set_error(error, error_size, "Opcion desconocida o repetida.");
            return false;
        }
    }

    if (!have_phrase || !have_upper) {
        set_error(error, error_size, "Faltan --phrase y/o --upper.");
        return false;
    }
    if (have_cipher_file == have_text) {
        set_error(error, error_size,
                  "Use --cipher-file (ataque real) o --text + --key (autoprueba), no ambos.");
        return false;
    }
    if (have_text && !have_key) {
        set_error(error, error_size, "--text requiere --key para la autoprueba.");
        return false;
    }

    const size_t phrase_len = strlen(parsed.phrase);
    if (have_text) {
        const size_t text_len = strlen(parsed.text);
        if (text_len == 0 || text_len > MAX_TEXT_BYTES) {
            set_error(error, error_size, "El texto debe contener entre 1 byte y 1 MiB.");
            return false;
        }
        if (phrase_len == 0 || phrase_len > text_len) {
            set_error(error, error_size, "La frase no puede estar vacia ni superar el texto.");
            return false;
        }
    } else if (phrase_len == 0) {
        set_error(error, error_size, "La frase no puede estar vacia.");
        return false;
    }

    if (parsed.upper == 0 || parsed.upper > DES_KEYSPACE_SIZE) {
        set_error(error, error_size, "--upper debe cumplir 0 < upper <= 2^56.");
        return false;
    }
    if (parsed.lower >= parsed.upper || (parsed.has_key && parsed.key >= parsed.upper)) {
        set_error(error, error_size, "Se requiere lower (y key) < upper <= 2^56.");
        return false;
    }
    *options = parsed;
    return true;
}

/* Lee un archivo binario completo (el texto cifrado de otro equipo). */
bool read_cipher_file(const char *path, unsigned char **buffer, size_t *length) {
    if (path == NULL || buffer == NULL || length == NULL) {
        return false;
    }
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return false;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return false;
    }
    const long size = ftell(f);
    if (size < 0 || (size_t)size == 0 || (size_t)size > MAX_TEXT_BYTES ||
        (size_t)size % DES_BLOCK_SIZE != 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return false;
    }
    unsigned char *data = malloc((size_t)size);
    if (data == NULL) {
        fclose(f);
        return false;
    }
    const size_t read_bytes = fread(data, 1, (size_t)size, f);
    fclose(f);
    if (read_bytes != (size_t)size) {
        free(data);
        return false;
    }
    *buffer = data;
    *length = (size_t)size;
    return true;
}

/* Busca una frase en un búfer que puede contener bytes nulos. */
static bool contains_bytes(const unsigned char *buffer, size_t buffer_len,
                          const char *needle) {
    const size_t needle_len = strlen(needle);
    if (needle_len == 0 || needle_len > buffer_len) {
        return false;
    }
    for (size_t offset = 0; offset <= buffer_len - needle_len; ++offset) {
        if (memcmp(buffer + offset, needle, needle_len) == 0) {
            return true;
        }
    }
    return false;
}

/* Descifra una candidata y busca la frase indicada. */
bool try_key(uint64_t key, const unsigned char *cipher, size_t cipher_len,
             const char *phrase) {
    if (cipher == NULL || cipher_len == 0 ||
        cipher_len % DES_BLOCK_SIZE != 0 || phrase == NULL ||
        phrase[0] == '\0' || cipher_len == SIZE_MAX) {
        return false;
    }

    unsigned char *candidate = malloc(cipher_len + 1U);
    if (candidate == NULL) {
        return false;
    }
    memcpy(candidate, cipher, cipher_len);

    size_t plain_len = cipher_len;
    bool matches = false;
    if (decrypt_buffer(key, candidate, cipher_len)) {
        /* Los archivos del foro pueden usar PKCS#7, NUL o espacio de relleno. */
        (void)remove_pkcs7_padding(candidate, cipher_len, &plain_len);
        matches = contains_bytes(candidate, plain_len, phrase);
    }
    free(candidate);
    return matches;
}

/* Recorre un rango de claves hasta encontrar una coincidencia. */
bool search_range(uint64_t lower, uint64_t upper,
                  const unsigned char *cipher, size_t cipher_len,
                  const char *phrase, uint64_t *found_key) {
    if (lower > upper || found_key == NULL) {
        return false;
    }
    for (uint64_t key = lower; key < upper; ++key) {
        if (try_key(key, cipher, cipher_len, phrase)) {
            *found_key = key;
            return true;
        }
    }
    return false;
}

#ifndef BRUTEFORCE_CORE_TEST
/* Muestra la forma correcta de ejecutar el programa. */
static void print_usage(const char *program) {
    fprintf(stderr,
            "Autoprueba:   %s --text TEXTO --phrase FRASE --key N --upper N\n"
            "Ataque real:  %s --cipher-file RUTA --phrase FRASE --upper N [--lower N]\n"
            "Condicion: 0 <= lower < upper <= 2^56.\n",
            program, program);
}

/* Obtiene el nombre del nodo actual para identificarlo en los logs. */
static void get_node_name(char *buffer, size_t size) {
    if (gethostname(buffer, size) != 0) {
        snprintf(buffer, size, "desconocido");
    }
    buffer[size - 1] = '\0';
}

int main(int argc, char **argv) {
    /* Inicializa MPI y obtiene la identidad del proceso actual. */
    if (MPI_Init(&argc, &argv) != MPI_SUCCESS) {
        fputs("No se pudo inicializar MPI.\n", stderr);
        return EXIT_FAILURE;
    }

    MPI_Comm comm = MPI_COMM_WORLD;
    int rank = 0;
    int process_count = 0;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &process_count);

    char node_name[256] = {0};
    get_node_name(node_name, sizeof(node_name));

    CliOptions options;
    char error[160] = {0};
    const int local_args_ok =
        parse_cli_options(argc, argv, &options, error, sizeof(error)) ? 1 : 0;
    int all_args_ok = 0;
    /* Verifica que todos los procesos reciban argumentos válidos. */
    MPI_Allreduce(&local_args_ok, &all_args_ok, 1, MPI_INT, MPI_MIN, comm);
    if (!all_args_ok) {
        if (rank == 0) {
            fprintf(stderr, "Error: %s\n", local_args_ok ?
                    "argumentos inconsistentes entre procesos" : error);
            print_usage(argv[0]);
        }
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    /* Prepara el texto cifrado: lo cifra localmente (autoprueba) o lo lee
     * de un archivo con el criptograma de otro equipo (ataque real). */
    unsigned char *cipher = NULL;
    size_t cipher_len = 0;
    int local_setup_ok = 0;
    if (options.cipher_file != NULL) {
        local_setup_ok = read_cipher_file(options.cipher_file, &cipher, &cipher_len) ? 1 : 0;
    } else {
        local_setup_ok =
            pad_plaintext((const unsigned char *)options.text, strlen(options.text),
                          &cipher, &cipher_len) &&
            encrypt_buffer(options.key, cipher, cipher_len);
    }
    int all_setup_ok = 0;
    /* Confirma que todos los procesos prepararon el texto correctamente. */
    MPI_Allreduce(&local_setup_ok, &all_setup_ok, 1, MPI_INT, MPI_MIN, comm);
    if (!all_setup_ok) {
        if (rank == 0) {
            fputs("No se pudo preparar el texto cifrado (revise --cipher-file).\n", stderr);
        }
        free(cipher);
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    uint64_t span_lower = 0;
    uint64_t span_upper = 0;
    if (!compute_range(options.upper - options.lower, process_count, rank,
                       &span_lower, &span_upper)) {
        free(cipher);
        MPI_Finalize();
        return EXIT_FAILURE;
    }
    const uint64_t lower = options.lower + span_lower;
    const uint64_t upper = options.lower + span_upper;
    printf("Nodo %s, proceso %d de %d: rango [%llu, %llu)\n", node_name, rank,
           process_count, (unsigned long long)lower, (unsigned long long)upper);
    fflush(stdout);

    const unsigned long long local_count =
        (unsigned long long)(upper - lower);
    unsigned long long max_count = 0;
    MPI_Allreduce(&local_count, &max_count, 1, MPI_UNSIGNED_LONG_LONG,
                  MPI_MAX, comm);

    MPI_Barrier(comm);
    const double start = MPI_Wtime();
    unsigned long long global_candidate = ULLONG_MAX;
    /* Busca las claves por lotes y sincroniza los resultados entre procesos. */
    for (uint64_t offset = 0; offset < (uint64_t)max_count;
         offset += SEARCH_BATCH_SIZE) {
        uint64_t batch_lower = lower + offset;
        uint64_t batch_upper = batch_lower;
        if (batch_lower < upper) {
            const uint64_t remaining = upper - batch_lower;
            batch_upper = batch_lower +
                (remaining < SEARCH_BATCH_SIZE ? remaining : SEARCH_BATCH_SIZE);
        }

        uint64_t local_found = UINT64_MAX;
        unsigned long long local_candidate = ULLONG_MAX;
        if (batch_lower < batch_upper &&
            search_range(batch_lower, batch_upper, cipher, cipher_len,
                         options.phrase, &local_found)) {
            local_candidate = (unsigned long long)local_found;
            printf("Nodo %s, proceso %d: encontro una candidata en %llu\n",
                   node_name, rank, local_candidate);
            fflush(stdout);
        }
        MPI_Allreduce(&local_candidate, &global_candidate, 1,
                      MPI_UNSIGNED_LONG_LONG, MPI_MIN, comm);
        if (global_candidate != ULLONG_MAX) {
            break;
        }
    }

    const double local_elapsed = MPI_Wtime() - start;
    double elapsed = 0.0;
    MPI_Reduce(&local_elapsed, &elapsed, 1, MPI_DOUBLE, MPI_MAX, 0, comm);

    int exit_code = EXIT_SUCCESS;
    /* El proceso cero muestra el resultado y valida el texto recuperado. */
    if (rank == 0) {
        if (global_candidate == ULLONG_MAX) {
            printf("Llave no encontrada en [%llu, %llu). Tiempo: %.6f s\n",
                   (unsigned long long)options.lower,
                   (unsigned long long)options.upper, elapsed);
            exit_code = 3;
        } else {
            size_t plain_len = 0;
            const uint64_t found_key = (uint64_t)global_candidate;
            const bool decrypted_ok = decrypt_buffer(found_key, cipher, cipher_len);
            const bool has_pkcs7 = decrypted_ok &&
                                   remove_pkcs7_padding(cipher, cipher_len, &plain_len);
            if (options.cipher_file != NULL) {
                /* Acepta formatos de relleno alternativos usados por otros equipos. */
                if (!decrypted_ok) {
                    fputs("La llave se encontro, pero no se pudo descifrar.\n", stderr);
                    exit_code = EXIT_FAILURE;
                } else {
                    if (!has_pkcs7) {
                        plain_len = cipher_len;
                        while (plain_len > 0 && cipher[plain_len - 1] == '\0') {
                            --plain_len;
                        }
                    }
                    printf("Llave encontrada: %llu\nTiempo: %.6f s\nTexto: %.*s\n",
                           global_candidate, elapsed, (int)plain_len, (char *)cipher);
                }
            } else if (!decrypted_ok || !has_pkcs7 ||
                      !plaintext_matches_expected(cipher, plain_len, options.text)) {
                fputs("La llave se encontro, pero no se pudo descifrar.\n", stderr);
                exit_code = EXIT_FAILURE;
            } else {
                printf("Llave encontrada: %llu\nTiempo: %.6f s\nTexto: %.*s\n",
                       global_candidate, elapsed, (int)plain_len, (char *)cipher);
            }
        }
    }

    MPI_Bcast(&exit_code, 1, MPI_INT, 0, comm);
    free(cipher);
    MPI_Finalize();
    return exit_code;
}
#endif
