# Teenage Mutant Ninja Turtles: The Arcade Game - Sega Genesis/Mega Drive Port

¡Bienvenido al proyecto de portabilidad/demake de **TMNT: The Arcade Game** para la Sega Genesis / Mega Drive! Este proyecto busca recrear la experiencia clásica del arcade original de 1989, aprovechando al máximo el hardware de los 16 bits de Sega utilizando el kit de desarrollo **SGDK**.

🎮 **Descarga y novedades:** [TMNT: Arcade Game - Megadrive Port by aguara.fox](https://valenzuelagustavo.itch.io/tmnt-arcade-game-megadrive-port) (Itch.io)

---

## 🤖 Nota de transparencia sobre el uso de IA

En este proyecto se utiliza **inteligencia artificial como una herramienta** de apoyo al desarrollo (por ejemplo, asistencia con el código en C, scripts auxiliares y documentación). La dirección del proyecto, las decisiones de diseño, las pruebas y la publicación son del desarrollador, y el juego se prueba y se ajusta a mano. **Los assets (sprites, escenarios, gráficos) no están generados con IA:** la gran mayoría está ripeada del arcade original (incluido material que estaba escondido en la ROM del arcade, como el caimán, aportado por Ray Castello), o fue hecha por los colaboradores nombrados en los créditos y por el propio desarrollador. Los scripts del repositorio solo convierten o recolorean esos assets para la paleta de la consola. Se aclara por honestidad hacia la comunidad.

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

2. 🐢 Créditos del port (los mismos que aparecen en los créditos finales del juego)

    Developer: Gustavo Valenzuela

    Música: SANSENPAI35

    Assets 2D y adaptación gráfica: Robson Ricardo

    Assets 2D y desarrollo: Ray Castello

    Tester: ROBOT-RANDOM

    Y gracias a toda la comunidad.

3. 🕹️ Staff del arcade original (1989), según figura en los créditos del juego

    Director: H. Ohyama

    Sub Director: K. Takabayashi

    Game Programmer: G. Suzuki

    Character Design: Moriyama 25

    Graphic Design: K. Hattori

    Visual Design: Y. Asano

    Animation Design: K. Yamashita

    Sound Director: S. Tasaka

    Sound Designer: Imo, Hideto

    Music Composer: M. Izumi / Miki Chang

    Hardware Design: S. Matsumoto

    Special Thanks: M. Inoue

## LICENCIA:

    Este es un proyecto no comercial hecho por fans con fines puramente educativos y de entretenimiento. Todos los derechos de los personajes, nombres y propiedades intelectuales pertenecen a sus respectivos dueños (Viacom/Nickelodeon/Konami).