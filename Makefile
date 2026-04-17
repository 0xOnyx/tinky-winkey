
# NMAKE — call with: nmake  |  nmake clean  |  nmake re

CC		    = cl
CFLAGS      = /nologo /Wall /WX /Zi /DUNICODE /D_UNICODE /Iinclude \
              /wd4820 /wd4668 /wd5045 /wd4710 /wd4711 /wd4255 /wd4996

LDFLAGS     = /nologo /DEBUG

LIBS        = advapi32.lib user32.lib kernel32.lib shell32.lib


SVC_OBJS    = svc\main.obj \
              svc\impersonate.obj \
              svc\scm.obj \
              svc\service.obj

WINKEY_OBJS = winkey\main.obj \
              #winkey\hook.obj \
              #winkey\foreground.obj \
             # winkey\logger.obj

all: svc.exe winkey.exe


svc.exe: $(SVC_OBJS)
	link $(LDFLAGS) /OUT:$@ $(SVC_OBJS) $(LIBS)


winkey.exe: $(WINKEY_OBJS)
    link $(LDFLAGS) /OUT:$@ $(WINKEY_OBJS) $(LIBS)


.c.obj:
	$(CC) $(CFLAGS) /c $< /Fo$@


clean:
	-del /Q svc\*.obj winkey\*.obj *.pdb *.ilk 2>nul


fclean: clean
	-del /Q svc.exe winkey.exe winkey.log 2>nul

re: fclean all