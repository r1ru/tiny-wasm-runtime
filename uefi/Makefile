# Before make, qemu, edk2/OvmfPkg and gnu-efi must be built and installed.

CC := gcc
LD := ld
OBJCOPY := objcopy
QEMU := qemu-system-x86_64

INC := /usr/local/include/
LIB := /usr/local/lib/

CFLAGS 	:= 	-I$(INC)/efi -Wno-error=pragmas -mno-red-zone -mno-avx -fPIE \
			-g -O2 -Wall -Wextra -Wno-pointer-sign -Werror -funsigned-char \
			-fshort-wchar -fno-strict-aliasing -ffreestanding -fno-stack-protector \
			-fno-stack-check -fno-merge-all-constants -DCONFIG_x86_64 -std=c11 \
			-DGNU_EFI_USE_MS_ABI -maccumulate-outgoing-args -D__KERNEL__ -c

LDFLAGS := 	-nostdlib --warn-common --no-undefined --fatal-warnings --build-id=sha1 \
			-z nocombreloc -shared -Bsymbolic -L$(LIB) $(LIB)crt0-efi-x86_64.o \
			/usr/lib/gcc/x86_64-linux-gnu/11/libgcc.a \
			-T $(LIB)elf_x86_64_efi.lds

OBJCOPYFLAGS :=	-j .text -j .sdata -j .data -j .dynamic -j .rodata -j .rel \
	    		-j .rela -j .rel.* -j .rela.* -j .rel* -j .rela* \
	    		-j .areloc -j .reloc --target efi-app-x86_64

QEMUFLAGS := -nographic -serial mon:stdio --no-reboot -net none \
            -bios $(HOME)/edk2/Build/OvmfX64/DEBUG_GCC5/FV/OVMF.fd

TARGETS := hello.efi fread.efi

.PHONY: build
build: $(TARGETS)

%.o : %.c
	$(CC) $(CFLAGS) -o $@ $<

%.so : %.o
	$(LD) $(LDFLAGS) $< -o $@ -lefi -lgnuefi

%.efi : %.so
	$(OBJCOPY) $(OBJCOPYFLAGS) $< $@

.PHONY: run
run: build
	$(QEMU) $(QEMUFLAGS) -drive format=raw,file=fat:rw:.

.PHONY: clean
clean:
	rm -f *.efi