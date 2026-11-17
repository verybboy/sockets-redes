/* Primera prueba TCP para Debian: una conexion y un mensaje.
 * El mensaje termina con '\n' o con EOF (cierre del envio del cliente).
 * Maximo: 1024 bytes de contenido, sin contar el '\n'.
 * TCP transporta bytes: una llamada a recv no equivale a un mensaje.
 */
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#define PUERTO_DEFECTO 18080
#define MAX_MENSAJE 1024

int main(int argc, char *argv[])
{
    long puerto = PUERTO_DEFECTO;
    int escucha = -1;
    int conexion = -1;
    int resultado = EXIT_FAILURE;
    struct sockaddr_in direccion = {0};
    char mensaje[MAX_MENSAJE];
    size_t usados = 0;
    const char respuesta[] = "RECIBIDO\n";
    size_t enviados = 0;

    if (argc > 2) {
        fprintf(stderr, "Uso: %s [puerto: 1-65535]\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (argc == 2) {
        char *fin;
        const char *p;

        /* Solo admitimos digitos decimales, sin signos ni espacios. */
        if (argv[1][0] == '\0') {
            fprintf(stderr, "Puerto vacio.\n");
            return EXIT_FAILURE;
        }
        for (p = argv[1]; *p != '\0'; ++p) {
            if (*p < '0' || *p > '9') {
                fprintf(stderr, "Puerto invalido: use un entero entre 1 y 65535.\n");
                return EXIT_FAILURE;
            }
        }
        errno = 0;
        puerto = strtol(argv[1], &fin, 10);
        if (errno != 0 || *fin != '\0' || puerto < 1 || puerto > 65535) {
            fprintf(stderr, "Puerto fuera del intervalo 1-65535.\n");
            return EXIT_FAILURE;
        }
    }

    /* socket crea un descriptor IPv4 (AF_INET) de flujo TCP (SOCK_STREAM).
     * El protocolo 0 selecciona el protocolo predeterminado: TCP. */
    escucha = socket(AF_INET, SOCK_STREAM, 0);
    if (escucha == -1) {
        perror("socket");
        goto cerrar;
    }

    /* sockaddr_in contiene la familia, el puerto y la direccion IPv4.
     * htons convierte los 16 bits del puerto al orden de bytes de red.
     * INADDR_LOOPBACK representa 127.0.0.1; htonl convierte sus 32 bits.
     * Asi solo se permiten conexiones desde esta misma maquina. */
    direccion.sin_family = AF_INET;
    direccion.sin_port = htons((uint16_t)puerto);
    direccion.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    /* bind asocia el socket a nuestra direccion y puerto locales. */
    if (bind(escucha, (const struct sockaddr *)&direccion,
             sizeof(direccion)) == -1) {
        perror("bind");
        goto cerrar;
    }
    /* listen convierte el socket en pasivo y permite encolar conexiones.
     * El backlog 1 no limita el numero total de clientes del programa. */
    if (listen(escucha, 1) == -1) {
        perror("listen");
        goto cerrar;
    }
    printf("Escuchando en 127.0.0.1:%ld\n", puerto);
    fflush(stdout);

    /* accept espera una conexion y devuelve OTRO descriptor para ella.
     * No necesitamos la direccion del cliente: usamos NULL, NULL.
     * Si una senal interrumpe la espera (EINTR), la repetimos. */
    do {
        conexion = accept(escucha, NULL, NULL);
    } while (conexion == -1 && errno == EINTR);
    if (conexion == -1) {
        perror("accept");
        goto cerrar;
    }

    for (;;) {
        char byte;
        ssize_t recibidos;

        /* recv devuelve bytes recibidos, 0 si hay EOF, o -1 si falla.
         * Leemos de uno en uno para reconocer el delimitador sin consumir
         * datos posteriores. Es sencillo para esta pequena prueba.
         * No esperamos que una sola recepcion contenga todo el mensaje. */
        do {
            recibidos = recv(conexion, &byte, 1, 0);
        } while (recibidos == -1 && errno == EINTR);
        if (recibidos == -1) {
            perror("recv");
            goto cerrar;
        }
        if (recibidos == 0) {
            if (usados == 0) {
                fprintf(stderr, "El cliente cerro su envio sin enviar un mensaje.\n");
                goto cerrar;
            }
            break;
        }
        if (byte == '\n') {
            break;
        }
        if (usados == sizeof(mensaje)) {
            fprintf(stderr, "Mensaje demasiado largo (maximo %d bytes).\n",
                    MAX_MENSAJE);
            goto cerrar;
        }
        mensaje[usados++] = byte;
    }

    /* fwrite usa la longitud real: recv no anade un terminador '\0'. */
    printf("Mensaje recibido (%zu bytes): ", usados);
    if (fwrite(mensaje, 1, usados, stdout) != usados ||
        putchar('\n') == EOF || fflush(stdout) == EOF) {
        perror("mostrar mensaje");
        goto cerrar;
    }

    /* send puede transmitir menos bytes que los solicitados: acumulamos
     * lo enviado hasta completar la respuesta, sin el '\0' de C.
     * MSG_NOSIGNAL (Linux) evita que SIGPIPE termine el proceso si el
     * cliente ha cerrado; podremos tratar el error y cerrar los sockets. */
    while (enviados < sizeof(respuesta) - 1) {
        ssize_t n = send(conexion, respuesta + enviados,
                         sizeof(respuesta) - 1 - enviados, MSG_NOSIGNAL);
        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("send");
            goto cerrar;
        }
        if (n == 0) {
            fprintf(stderr, "send no pudo avanzar.\n");
            goto cerrar;
        }
        enviados += (size_t)n;
    }
    resultado = EXIT_SUCCESS;

cerrar:
    /* close libera cada descriptor abierto, tanto en exito como en error.
     * En Linux no se reintenta close si falla: el descriptor ya se libera
     * incluso con EINTR y podria haberse reutilizado. */
    if (conexion != -1 && close(conexion) == -1) {
        perror("close conexion");
        resultado = EXIT_FAILURE;
    }
    if (escucha != -1 && close(escucha) == -1) {
        perror("close escucha");
        resultado = EXIT_FAILURE;
    }
    return resultado;
}
