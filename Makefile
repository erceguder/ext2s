mount:	copy.img fs-root
	fuseext2 -o rw+ -o use_ino copy.img fs-root
unmount:
	fusermount -u fs-root
