CC      := gcc
LD      := ld
CFLAGS  := -std=c11 -ffreestanding -fno-stack-protector -fno-pic -fpie \
           -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -mcmodel=kernel \
           -Wall -Wextra -Ikernel -Iexternal
LDFLAGS := -m elf_x86_64 -nostdlib -static -pie --no-dynamic-linker \
           -z text -z max-page-size=0x1000 -T kernel/arch/x86_64/linker.ld

SRCS    := $(shell find kernel -name '*.c')
OBJS    := $(SRCS:.c=.o)

all: arechi.iso

limine:
	git clone https://github.com --branch=v8.x-binary --depth=1
	$(MAKE) -C limine

kernel.elf: $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $@

arechi.iso: limine kernel.elf limine.conf
	mkdir -p iso_root/boot/limine iso_root/EFI/BOOT
	cp kernel.elf iso_root/boot/
	cp limine.conf iso_root/boot/limine/
	cp limine/limine-bios.sys limine/limine-bios-cd.bin limine/limine-uefi-cd.bin iso_root/boot/limine/
	cp limine/BOOTX64.EFI iso_root/EFI/BOOT/
	xorriso -as mkisofs -b boot/limine/limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table \
		--efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		iso_root -o $@
	./limine/limine bios-install $@

run: arechi.iso
	qemu-system-x86_64 -cdrom arechi.iso -serial stdio -m 512M

clean:
	rm -rf $(OBJS) kernel.elf arechi.iso iso_root
