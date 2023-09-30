# Before make, qemu and edk2/OvmfPkg must be built and installed.
QEMU := qemu-system-x86_64
QEMUFLAGS := -nographic -serial mon:stdio --no-reboot -net none \
            -bios $(HOME)/edk2/Build/OvmfX64/DEBUG_GCC5/FV/OVMF.fd
.PHONY: run
run:
	$(QEMU) $(QEMUFLAGS) -drive format=raw,file=fat:rw:uefi