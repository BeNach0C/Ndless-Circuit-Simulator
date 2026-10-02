DEBUG = FALSE

GCC = nspire-gcc
AS  = nspire-as
GXX = nspire-g++
LD  = nspire-ld
GENZEHN = genzehn

GCCFLAGS = -Wall -W -marm
LDFLAGS = -lSDL_gfx -lSDL -lndls
ZEHNFLAGS = --name "NaiveCirc"

ifeq ($(DEBUG),FALSE)
	GCCFLAGS += -O3 -fomit-frame-pointer -flto
	LDFLAGS += -flto
else
	GCCFLAGS += -O0 -g
endif

OBJS = MNAEngine.o Components.o CircuitGui.o main.o
EXE = NaiveCircuitSimulator
DISTDIR = .

all: $(EXE).tns

%.o: %.cpp
	$(GXX) $(GCCFLAGS) -c $< -o $@

$(EXE).elf: $(OBJS)
	$(LD) $^ -o $(DISTDIR)/$@ $(LDFLAGS)

$(EXE).tns: $(EXE).elf
	$(GENZEHN) --input $(DISTDIR)/$^ --output $(DISTDIR)/$@.zehn $(ZEHNFLAGS)
	make-prg $(DISTDIR)/$@.zehn $(DISTDIR)/$@
	rm -f $(DISTDIR)/$@.zehn

clean:
	rm -f *.o $(DISTDIR)/$(EXE).tns $(DISTDIR)/$(EXE).elf $(DISTDIR)/$(EXE).zehn
