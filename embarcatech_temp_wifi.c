// Biblioteca padrão de entrada e saída
#include <stdio.h>

// Biblioteca para manipulação de strings
#include <string.h>

// Biblioteca para alocação dinâmica de memória
#include <stdlib.h>

// Biblioteca principal do Raspberry Pi Pico
#include "pico/stdlib.h"

// Biblioteca para leitura analógica
#include "hardware/adc.h"

// Biblioteca para PWM
#include "hardware/pwm.h"

// Biblioteca do Wi-Fi do Pico W
#include "pico/cyw43_arch.h"

// Biblioteca TCP do lwIP
#include "lwip/tcp.h"

// Biblioteca da interface de rede
#include "lwip/netif.h"

// Biblioteca para IP v4
#include "lwip/ip4_addr.h"

// Nome da rede Wi-Fi
#define WIFI_SSID "brisa-3240146"

// Senha da rede Wi-Fi
#define WIFI_PASSWORD "93ohnlb7"

// Pino do LED vermelho da BitDogLab
#define LED_VERMELHO 13

// Pino do buzzer
#define BUZZER_PIN 10

// Limite de temperatura
#define LIMITE_TEMPERATURA 32.7f

// Intervalo entre leituras da temperatura
#define INTERVALO_LEITURA_MS 3000

// Variável global para armazenar a temperatura
float temperatura_c = 0.0f;

// Variável para indicar se o alerta está ativo
bool alerta_ativo = false;

// Variável para indicar se o alerta foi silenciado pela página
bool alerta_silenciado = false;

// Ponteiro global do servidor TCP
struct tcp_pcb *servidor_pcb = NULL;

// Protótipos das funções
void configurar_pwm(uint gpio, uint16_t wrap);
void set_pwm_duty(uint gpio, uint16_t duty);
void ligar_alerta(void);
void desligar_alerta(void);
float ler_temperatura(void);
void montar_pagina_html(char *html, size_t tamanho);
bool iniciar_servidor_http(void);

// Protótipos das funções de callback do servidor
static err_t callback_http_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
static err_t callback_http_accept(void *arg, struct tcp_pcb *newpcb, err_t err);

// Função para configurar PWM em um pino
void configurar_pwm(uint gpio, uint16_t wrap) {
    gpio_set_function(gpio, GPIO_FUNC_PWM);

    uint slice = pwm_gpio_to_slice_num(gpio);

    pwm_set_wrap(slice, wrap);

    if (gpio == BUZZER_PIN) {
        pwm_set_clkdiv(slice, 50.0f);
    } else {
        pwm_set_clkdiv(slice, 4.0f);
    }

    pwm_set_gpio_level(gpio, 0);
    pwm_set_enabled(slice, true);
}

// Função para definir o duty cycle do PWM
void set_pwm_duty(uint gpio, uint16_t duty) {
    pwm_set_gpio_level(gpio, duty);
}

// Função que gera o efeito de sirene
void ligar_alerta(void) {
    static bool estado = false;

    if (estado) {
        set_pwm_duty(BUZZER_PIN, 2500);
        set_pwm_duty(LED_VERMELHO, 45000);
    } else {
        set_pwm_duty(BUZZER_PIN, 1200);
        set_pwm_duty(LED_VERMELHO, 20000);
    }

    estado = !estado;
}

// Função para desligar LED e buzzer
void desligar_alerta(void) {
    set_pwm_duty(BUZZER_PIN, 0);
    set_pwm_duty(LED_VERMELHO, 0);
}

// Função para ler a temperatura interna do RP2040
float ler_temperatura(void) {
    adc_select_input(4);

    uint16_t leitura = adc_read();

    const float fator_conversao = 3.3f / (1 << 12);
    float tensao = leitura * fator_conversao;

    float temperatura = 27.0f - (tensao - 0.706f) / 0.001721f;

    return temperatura;
}

// Função que monta a página HTML para o celular
void montar_pagina_html(char *html, size_t tamanho) {
    char status_temp[100];
    char status_alerta[100];
    char botao[250];

    if (temperatura_c > LIMITE_TEMPERATURA) {
        snprintf(status_temp, sizeof(status_temp), "Perigo! Temperatura acima do limite");
    } else {
        snprintf(status_temp, sizeof(status_temp), "Temperatura segura");
    }

    if (temperatura_c > LIMITE_TEMPERATURA) {
        if (alerta_silenciado) {
            snprintf(status_alerta, sizeof(status_alerta), "Alerta silenciado pelo usuario");
        } else {
            snprintf(status_alerta, sizeof(status_alerta), "Alerta ativo");
        }
    } else {
        snprintf(status_alerta, sizeof(status_alerta), "Sistema normal");
    }

    if (temperatura_c > LIMITE_TEMPERATURA && !alerta_silenciado) {
        snprintf(botao, sizeof(botao),
                 "<a href=\"/desligar\"><button style=\"font-size:20px;padding:15px 25px;\">Desligar buzzer e LED</button></a>");
    } else {
        snprintf(botao, sizeof(botao), "");
    }

    snprintf(html, tamanho,
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/html; charset=UTF-8\r\n"
             "Connection: close\r\n"
             "Refresh: 3\r\n"
             "\r\n"
             "<!DOCTYPE html>"
             "<html>"
             "<head>"
             "<meta charset=\"UTF-8\">"
             "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
             "<title>Monitoramento de Temperatura</title>"
             "</head>"
             "<body style=\"font-family:Arial;text-align:center;margin-top:40px;\">"
             "<h1>Projeto EmbarcaTech</h1>"
             "<h2>Monitoramento de Temperatura</h2>"
             "<p style=\"font-size:28px;\">Temperatura: %.2f C</p>"
             "<p style=\"font-size:22px;\">%s</p>"
             "<p style=\"font-size:22px;\">%s</p>"
             "%s"
             "</body>"
             "</html>",
             temperatura_c, status_temp, status_alerta, botao);
}

// Callback chamada quando chega uma requisição HTTP
static err_t callback_http_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (p == NULL) {
        tcp_close(tpcb);
        return ERR_OK;
    }

    char *requisicao = (char *)malloc(p->tot_len + 1);
    if (requisicao == NULL) {
        pbuf_free(p);
        tcp_close(tpcb);
        return ERR_MEM;
    }

    pbuf_copy_partial(p, requisicao, p->tot_len, 0);
    requisicao[p->tot_len] = '\0';

    if (strstr(requisicao, "GET /desligar") != NULL) {
        alerta_silenciado = true;
        alerta_ativo = false;
        desligar_alerta();
        printf("Alerta desligado pela pagina web.\n");
    }

    char resposta[1600];
    memset(resposta, 0, sizeof(resposta));
    montar_pagina_html(resposta, sizeof(resposta));

    tcp_write(tpcb, resposta, strlen(resposta), TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);

    tcp_recved(tpcb, p->tot_len);

    free(requisicao);
    pbuf_free(p);

    tcp_close(tpcb);

    return ERR_OK;
}

// Callback chamada quando um cliente conecta no servidor
static err_t callback_http_accept(void *arg, struct tcp_pcb *newpcb, err_t err) {
    tcp_recv(newpcb, callback_http_recv);
    return ERR_OK;
}

// Função para iniciar o servidor HTTP
bool iniciar_servidor_http(void) {
    servidor_pcb = tcp_new();

    if (servidor_pcb == NULL) {
        printf("Erro ao criar servidor TCP.\n");
        return false;
    }

    if (tcp_bind(servidor_pcb, IP_ADDR_ANY, 80) != ERR_OK) {
        printf("Erro ao fazer bind na porta 80.\n");
        tcp_close(servidor_pcb);
        servidor_pcb = NULL;
        return false;
    }

    servidor_pcb = tcp_listen(servidor_pcb);

    if (servidor_pcb == NULL) {
        printf("Erro ao colocar servidor em escuta.\n");
        return false;
    }

    tcp_accept(servidor_pcb, callback_http_accept);

    printf("Servidor HTTP iniciado na porta 80.\n");
    return true;
}

// Função principal
int main(void) {
    stdio_init_all();

    sleep_ms(2000);
    printf("Iniciando projeto de temperatura com Wi-Fi...\n");

    adc_init();
    adc_set_temp_sensor_enabled(true);

    configurar_pwm(LED_VERMELHO, 65535);
    configurar_pwm(BUZZER_PIN, 5000);

    desligar_alerta();

    if (cyw43_arch_init()) {
        printf("Erro ao iniciar Wi-Fi.\n");
        return 1;
    }

    cyw43_arch_enable_sta_mode();

    printf("Conectando ao Wi-Fi...\n");

    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("Falha ao conectar ao Wi-Fi.\n");
        cyw43_arch_deinit();
        return 1;
    }

    printf("Wi-Fi conectado com sucesso.\n");

    if (!iniciar_servidor_http()) {
        cyw43_arch_deinit();
        return 1;
    }

    absolute_time_t proxima_leitura = make_timeout_time_ms(INTERVALO_LEITURA_MS);
    absolute_time_t proxima_sirene = make_timeout_time_ms(300);

    while (true) {
        if (absolute_time_diff_us(get_absolute_time(), proxima_leitura) <= 0) {
            temperatura_c = ler_temperatura();

            const ip4_addr_t *ip = netif_ip4_addr(netif_default);
            printf("IP: %s | Temperatura: %.2f C\n", ip4addr_ntoa(ip), temperatura_c);

            if (temperatura_c > LIMITE_TEMPERATURA) {
                if (!alerta_silenciado) {
                    alerta_ativo = true;
                } else {
                    alerta_ativo = false;
                    desligar_alerta();
                }
            } else {
                alerta_ativo = false;
                alerta_silenciado = false;
                desligar_alerta();
            }

            proxima_leitura = make_timeout_time_ms(INTERVALO_LEITURA_MS);
        }

        if (alerta_ativo) {
            if (absolute_time_diff_us(get_absolute_time(), proxima_sirene) <= 0) {
                ligar_alerta();
                proxima_sirene = make_timeout_time_ms(300);
            }
        }

        sleep_ms(20);
    }

    cyw43_arch_deinit();
    return 0;
}
