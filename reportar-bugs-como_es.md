# El Ingles como idioma obligatorio

El idioma ingles es utilizado de forma estándar en la producción de software. Este documento está destinado a aquellos que no dominan el ingles, pero necesitan o quieren comunicar alguna incidencia relativa a un presunto mal funcionamiento del propio software: los llamados bugs.
Tenga en cuenta que la persona que escribió el código que puede contener el fallo que usted cree observar comprenderá el idioma ingles, pero no necesariamente los cientos de idiomas existentes.

# El archivo ISSUE_TEMPLATE.md

Ese archivo contienen una plantilla que deberá utilizar para comunicar problemas.
Lo que sigue es una traducción del contenido de ISSUE_TEMPLATE.md, pero NO LA UTILICE.
Recuerde, además, que la información que usted aporte también debe estar escrita en ingles.
Si tiene problemas con ese idioma, le sugerimos se dirija al foro https://forum.linuxcnc.org/. 
Allí hay secciones para varios lenguajes. Siéntase libre de expresarse en su idioma nativo en la sección que corresponda.

Aquí empieza la traducción de ISSUE_TEMPLATE.md

-----------------------------------------------------------------------------------------

# El rastreador de problemas no es un foro de soporte

El rastreador de problemas de LinuxCNC sirve para comunicar errores en el software.
Si tiene alguna pregunta sobre cómo utilizar el software, utilice uno de los métodos detallados en la página de soporte de nuestra comunidad: http://linuxcnc.org/community/

(elimine esta sección antes de enviar su informe de errores)

## Estos son los pasos que sigo para reproducir el problema:

 1.
 2.
 3.

## Esto es lo que esperaba que sucediera:

## Esto es lo que sucedió en su lugar:

## Funcionaba correctamente antes de que:
(Si el comportamiento cambió después de realizar un cambio particular en el hardware o
software, describa el cambio que cree que es responsable. Por ejemplo, "después de actualizar
desde LinuxCNC 2.7.3 a 2.7.4 ")

## Información sobre mi hardware y software:

 * Estoy usando esta distribución y versión de Linux (a menudo mostrada por `lsb_release -a`):
 * Estoy usando la versión del kernel (mostrada por `uname -a`):
 * Estoy corriendo ...
   * [ ] Una versión binaria de linuxcnc.org (incluido buildbot.linuxcnc.org)
   * [ ] Un binario que construí yo mismo
   * [ ] Una versión binaria de alguna otra fuente además de linuxcnc.org
 * Estoy usando esta versión de LinuxCNC (se muestra en el administrador de paquetes o, para las versiones de git, `scripts/get-version-from-git`):
 * Estoy usando esta interfaz de usuario (GUI) (por ejemplo, AXIS, Touchy, gmoccapy, etc.):
 * Estoy usando este chipset y proveedor de hardware de interfaz (por ejemplo, puerto paralelo, puerto ethernet, tarjeta FPGA):
 
