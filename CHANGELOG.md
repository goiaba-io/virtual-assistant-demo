# Changelog

Todas as mudanças notáveis neste projeto serão documentadas neste arquivo.

O formato é baseado em [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [1.1.0] - 2025-09-25

Esta versão introduz um sistema de feedback visual usando um LED RGB, melhorando significativamente a interatividade e a capacidade de depuração do assistente virtual.

### Added (Adicionado)
- **Indicador de Status com LED RGB:** Integrado suporte a um LED WS2812 (NeoPixel) para fornecer feedback visual sobre o estado do assistente virtual.
- **Módulo de Controle `rgb_led`:** Criado um novo módulo de abstração (`rgb_led.h`, `rgb_led.cpp`) que atua como um wrapper em C para o componente `led_strip` do ESP-IDF, facilitando o controle do LED.
- **Novos Estados Visuais:** Definidos múltiplos estados para o LED, incluindo:
    - `Pronto` (Verde sólido): O assistente está ocioso e pronto para receber comandos.
    - `Pensando` (Amarelo ou Azul piscando lentamente): O assistente está processando o áudio captado pelo microfone.
    - `Respondendo` (Azul piscando rapidamente): O assistente está falando e reproduzindo áudio.
    - `Erro` (Vermelho piscando): Indica falhas de conexão ou erros críticos.
