/* Diagnostico academico de interoperabilidad para archivos DES/ECB.
 * Compilar con mpicc -O2 -DDES_KEY_LAYOUT=N scripts/diagnosticar_des.c -lcrypto.
 * Reutiliza la conversion de llave; filtra texto legible sin exigir frase exacta.
 */
#define main application_main
#include "../src/bruteforce_mpi.c"
#undef main

static size_t ascii_count(const unsigned char *data, size_t size) {
    size_t result = 0;
    for (size_t i = 0; i < size; ++i)
        result += (data[i] >= 32 && data[i] <= 126) ||
                  data[i] == '\n' || data[i] == '\r' || data[i] == '\t';
    return result;
}

static bool plausible_text(const unsigned char *data, size_t length, size_t percent) {
    size_t ascii = ascii_count(data, length), zeros = 0;
    for (size_t i = 0; i < length; ++i) zeros += data[i] == 0;
    return ascii * 100 >= length * percent ||
           (ascii * 100 >= length * 40 && (ascii + zeros) * 100 >= length * percent);
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    unsigned char *cipher = NULL;
    size_t length = 0;
    uint64_t limit = 0;
    unsigned char known_prefix[32];
    unsigned char *known = NULL;
    if ((argc != 3 && argc != 4) || !parse_uint56(argv[2], &limit) ||
        !read_cipher_file(argv[1], &cipher, &length) || length < 32)
        MPI_Abort(MPI_COMM_WORLD, 2);
    if (argc == 4) {
        FILE *input = fopen(argv[3], "rb");
        if (!input || fread(known_prefix, 1, 32, input) != 32)
            MPI_Abort(MPI_COMM_WORLD, 2);
        fclose(input);
        known = known_prefix;
    }
    unsigned char *plain = malloc(length);
    if (!plain) MPI_Abort(MPI_COMM_WORLD, 2);
    uint64_t low, high;
    compute_range(limit, size, rank, &low, &high);
    const double start = MPI_Wtime();
    if (rank == 0) {
        printf("DIAGNOSTICO layout=%d procesos=%d rango=[0,%llu)\n",
               DES_KEY_LAYOUT, size, (unsigned long long)limit);
        fflush(stdout);
    }
    int candidates = 0;
    for (uint64_t key = low; key < high; ++key) {
        memcpy(plain, cipher, 32);
        decrypt_buffer(key, plain, 32);
        /* CBC: el segundo bloque no depende del IV inicial desconocido. */
        if (known && memcmp(plain + 8, known + 8, 24) != 0) continue;
        if (!known && !plausible_text(plain, 32, 75)) {
            const size_t middle = ((length - 32) / 16) * 8;
            memcpy(plain, cipher + middle, 32);
            decrypt_buffer(key, plain, 32);
            if (!plausible_text(plain, 32, 75)) continue;
        }
        memcpy(plain, cipher, length);
        decrypt_buffer(key, plain, length);
        if (!known && !plausible_text(plain, length, 85)) continue;
        unsigned char bytes[8];
        logical_key_to_des(key, bytes);
#ifdef DES_RPC_PARITY
        for (size_t i = 0; i < 8; ++i) bytes[i] &= 0x7f;
#endif
        DES_set_odd_parity((DES_cblock *)bytes);
        printf("CANDIDATA layout=%d rank=%d entero=%llu hex=",
               DES_KEY_LAYOUT, rank, (unsigned long long)key);
        for (size_t i = 0; i < 8; ++i) printf("%02x", bytes[i]);
        printf("\nTexto: %.*s\n", (int)length, (char *)plain);
        char path[160];
        snprintf(path, sizeof(path), "/tmp/des-candidata-layout%d-key%llu.txt",
                 DES_KEY_LAYOUT, (unsigned long long)key);
        FILE *out = fopen(path, "wb");
        if (out) { fwrite(plain, 1, length, out); fclose(out); }
        fflush(stdout);
        ++candidates;
        /* Varias llaves enteras pueden compartir los mismos bits DES efectivos. */
        break;
    }
    int total = 0;
    MPI_Reduce(&candidates, &total, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
    if (rank == 0) printf("FIN layout=%d candidatas=%d tiempo=%.6f\n",
                           DES_KEY_LAYOUT, total, MPI_Wtime() - start);
    free(plain);
    free(cipher);
    MPI_Finalize();
    return 0;
}
