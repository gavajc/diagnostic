# Network & System Diagnostic Daemon (`diagnostic`)

Un demonio nativo industrial de telemetría y diagnóstico de red de alto rendimiento desarrollado en **C++17 puro**, optimizado específicamente para arquitecturas embebidas limitadas (**MIPS/MIPS64**) en entornos **OpenWrt**.

Este binario sustituye por completo a los scripts de telemetría tradicionales basados en Python, eliminando el severo *startup overhead* y reduciendo el consumo de recursos en memoria RAM y almacenamiento Flash a una fracción insignificante.

## 🚀 Características Clave

*   **Rendimiento Fulminante:** Recolección de métricas del sistema en microsegundos mediante llamadas directas a la memoria del kernel (`sysinfo`, `getifaddrs`).
*   **Asincronía de Alto Nivel:** Manejo de señales del sistema operativo (`SIGINT`/`SIGTERM`) mediante el subsistema asíncrono **`boost::asio::signal_set`** respaldado por el mecanismo nativo `signalfd` y `epoll` de Linux. El hilo principal se mantiene libre para la lógica de negocio.
*   **Espera Eficiente:** Dormido profundo con costo cero de CPU a través de **`std::condition_variable`**. Despierta instantáneamente ante señales del sistema operativo para un apagado limpio (RAII).
*   **Protección de Almacenamiento Flash:** Filtro estricto en RAM para actualizaciones del intervalo. El demonio **solo escribe en la Flash ROM vía UCI si el intervalo de tiempo cambia**, protegiendo las celdas de memoria del router contra la degradación física.
*   **Trazabilidad Industrial:** Integración directa con el demonio de logs nativo de OpenWrt (`logread`) mediante la API estándar de **`<syslog.h>`**, clasificando eventos críticos, advertencias y notas de red.

---

## 🏗️ Arquitectura del Software

El proyecto sigue una estructura atómica y desacoplada basada en el **Principio de Responsabilidad Única (SRP)**:

1.  **`SysUtils` (Utilidades del Sistema):** Biblioteca *header-only* estática pura encargada de interrogar al kernel (uptime, carga, interfaces de red, túneles VPN, PIDs en `/proc` y pruebas síncronas de latencia/DNS).
2.  **`ConfigManager` (Persistencia UCI):** *Wrapper* orientado a objetos que encapsula la API nativa en C de `libuci` bajo el patrón RAII. Abre, lee las 5 secciones de configuración, inyecta los datos en memoria y libera el contexto de inmediato para no desperdiciar RAM.
3.  **`HttpClient` (Comunicaciones Boost.Beast):** Cliente HTTP/HTTPS síncrono ultra esbelto que implementa el patrón **Pimpl** (*Pointer to Implementation*) para aislar los pesados encabezados de red del flujo público.
4.  **`DaemonHandler` (Orquestador Central):** Motor principal *header-only* que gobierna el ciclo de vida del demonio: vacía la cola local, ejecuta la recolección, administra el almacenamiento temporal en la RAM-disk (`/tmp/diagnostic/`) con un límite estricto de `N` reportes, y atiende las señales de red.

---

## 📦 Estructura del Archivo de Configuración (`/etc/config/diagnostic`)

El demonio es completamente dinámico y obedece en caliente a las directrices dictadas por el archivo de configuración UCI de OpenWrt:

```ini
config sysdiagnostic 'system'
	option mac_interface 'br-lan'

config sysdiagnostic 'dns_test'
	option host 'openwrt.org'

config sysdiagnostic 'latency_test'
	option host 'openwrt.org'
	option port '443'

config sysdiagnostic 'api'
	option url 'http://tu-servidor-api.local'
	option token 'tu_bearer_token_secreto_aqui'

config sysdiagnostic 'schedule'
	option check_interval '60'
	option max_queued_reports '10'
```

---

## 🔄 Mecanismo de Almacenamiento Local (Cola Desconectada)

Si el enlace hacia la API REST se cae, o si no hay ninguna API configurada en UCI, el demonio activa de forma automática su modo offline:
*   Almacena los reportes en la RAM-disk local: `/tmp/diagnostic/report_<timestamp>.json`.
*   Monitorea la cantidad de archivos existentes basándose en la opción `max_queued_reports`.
*   Si se alcanza el límite máximo `N`, el sistema ordena los archivos cronológicamente, **elimina el reporte más antiguo de la memoria Flash/RAM** e inserta el nuevo, blindando al router contra desbordamientos de almacenamiento.
*   En cuanto el enlace de red se recupera, el demonio vacía secuencialmente (del más viejo al más nuevo) toda la cola pendiente antes de enviar la telemetría actual.

---

## 🛠️ Requisitos de Compilación (Toolchain de OpenWrt)

El proyecto requiere enlazarse contra las librerías nativas del core del sistema operativo y las primitivas de hilos de C++17:

*   **Bandera de Estándar:** `-std=c++17`
*   **Banderas de Enlace:** `-luci -lubox -pthread`

El ejecutable final se genera bajo el nombre de **`diagnostic`** listo para ser administrado por el supervisor de servicios `procd`.

