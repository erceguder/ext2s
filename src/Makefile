all: ext2fs.h ext2fs.cpp
	@g++ -o ext2fs ext2fs.cpp

dup: ext2fs ../copy.img
	@./ext2fs dup ../copy.img 14 2/5112bytefile.txt
	@./ext2fs dup ../copy.img 12 13/vi_calisma.txt

test: ext2fs.h ext2fs.cpp ext2fs ../copy.img
	@make all
	@make dup

abs_path:
	@./ext2fs dup ../copy.img /folder/ostechnix.txt 2/5112bytefile.txt

clear_img:
	@rm -rf ../copy.img
#	@rm -rf ../fs-root

prepare_img:
#	dd if=/dev/zero of=../example.img bs=1024 count=512
#	mke2fs -t ext2 -b 2048 -N 64 ../example.img
#	mkdir ../fs-root
#	chmod g+rx ../fs-root
#	fuseext2 -o rw+ -o use_ino ../example.img ../fs-root
#	echo "123456789" > ../fs-root/deneme.txt
#	mkdir ../fs-root/folder	
#	dd if=/dev/zero of=../fs-root/folder/ostechnix.txt bs=5112 count=1
#	chmod g+x ../..
#	chmod g+x ../
#	fusermount -u ../fs-root
#	../robin convert ../example.img
	@cp ../example.img ../copy.img