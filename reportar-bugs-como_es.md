# El Ingles como idioma obligatorio

El idioma ingles es utilizado de forma estandar en la produccion de software. Este documento está destinado a aquellos que no dominan el ingles, pero necesitan o quieren comunicar alguna incidencia relativa a un presunto mal funcionamiento del propio software: los llamados bugs.
Tenga en cuenta que la persona que escribio el codigo que puede contener el fallo que usted cree observar comprendera el idioma ingles, pero no necesariamente los cientos de idiomas existentes.

# El archivo ISSUE_TEMPLATE.md

Este archivo contienen una plantilla que deberá utilizar para comunicar problemas.
Lo que sigue es una traduccion del contenido de ISSUE_TEMPLATE.md, pero NO LA UTILICE.
Recuerde, ademas, que la informacion que usted aporte tambien debe estrar escrita en ingles.
Si tiene problemas con ese idioma, le sugerimos se dirija al foro https://forum.linuxcnc.org/
Alli hay secciones para varios idiomas. Sientase libre de expresarse en su idioma nativo en la seccion que corresponda.

Aqui empieza la traduccion de ISSUE_TEMPLATE.md

-----------------------------------------------------------------------------------------

# The issue tracker is not a support forum

The LinuxCNC issue tracker is to report bugs in the software.
If you have a question about how to use the software, use one of the other methods detailed on our community support page: http://linuxcnc.org/community/

(delete this section before submitting your bug report)

## Here are the steps I follow to reproduce the issue:

 1.
 2.
 3.

## This is what I expected to happen:

## This is what happened instead:

## It worked properly before this:
(If the behavior changed after making a particular change in hardware or
software, describe the change you think is responsible.  E.g., "after upgrading
from LinuxCNC 2.7.3 to 2.7.4")

## Information about my hardware and software:

 * I am using this Linux distribution and version (often, shown by `lsb_release -a`):
 * I am using this kernel version (shown by `uname -a`):
 * I am running ...
   * [ ] A binary version from linuxcnc.org (including buildbot.linuxcnc.org)
   * [ ] A binary I built myself
   * [ ] A binary version from some other source besides linuxcnc.org
 * I am using this LinuxCNC version (shown in package manager or, for git versions, `scripts/get-version-from-git`):
 * I am using this user interface (GUI) (e.g., AXIS, Touchy, gmoccapy, etc):
 * I am using this interface hardware vendor and chipset (e.g., parallel port, ethernet port, FPGA card): 
