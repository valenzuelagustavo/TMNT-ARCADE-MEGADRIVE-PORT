# Teenage Mutant Ninja Turtles: The Arcade Game - Sega Genesis/Mega Drive Port

¡Bienvenido al proyecto de portabilidad/demake de **TMNT: The Arcade Game** para la Sega Genesis / Mega Drive! Este proyecto busca recrear la experiencia clásica del arcade original de 1989, aprovechando al máximo el hardware de los 16 bits de Sega utilizando el kit de desarrollo **SGDK**.

---

## 🚀 Características del Proyecto

* **Fidelidad al Arcade:** Recreación de los sprites, escenarios y mecánicas del juego original.
* **Optimización de Color:** Paletas adaptadas para lucir vibrantes dentro del límite de 64 colores simultáneos de la consola.
* **Sonido FM:** Banda sonora y efectos convertidos para el chip de sonido YM2612.
* **Desarrollado en C:** Compilado utilizando **SGDK** (Sega Genesis Development Kit).

## 🛠️ Requisitos para Desarrollo

Si deseas compilar el proyecto por ti mismo o contribuir, necesitarás:

* **SGDK** (Se recomienda la versión más reciente).
* **VS Code** con la extensión **Genesis Code** (opcional, pero recomendado).
* Un emulador compatible con Genesis/Mega Drive (como *BlastEm*, *Kega Fusion* o *Genesis Plus GX*) para pruebas.

## 📦 Cómo Compilar

1. Clona este repositorio:
   ```bash
   git clone [https://github.com/TU_USUARIO/TU_REPOSITORIO.git](https://github.com/TU_USUARIO/TU_REPOSITORIO.git)

2. Asegúrate de tener configurada la variable de entorno GDK apuntando a tu instalación de SGDK.

3. 📂 Estructura del Proyecto
    /src: Código fuente en C (.c y .h).

    /res: Recursos del juego (imágenes .png, mapas de tiles, audio, y archivos .res).

    /inc: Archivos de cabecera adicionales.

    /out: Archivos binarios y la ROM compilada (generada automáticamente).

## AGRADECIMIENTOS:

1. 👥 Créditos y Agradecimientos

    Konami: Creadores del juego arcade original.

    Stephane Dallongeville: Por el increíble desarrollo de SGDK.

    Comunidad de Sega Retro / Spriters Resource: Por la preservación de assets y documentación de hardware.

## LICENCIA:

    Este es un proyecto no comercial hecho por fans con fines puramente educativos y de entretenimiento. Todos los derechos de los personajes, nombres y propiedades intelectuales pertenecen a sus respectivos dueños (Viacom/Nickelodeon/Konami).
