cl -W3 -D_CRT_SECURE_NO_WARNINGS pasm.c pasmpp.c pasmexp.c pasmop.c pasmdot.c pasmstruct.c pasmmacro.c /Febin\pasm.exe
del *.obj
