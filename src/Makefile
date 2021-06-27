all: ext2fs.h ext2fs.cpp
	@g++ -o ext2sutils ext2fs.cpp

dup: ext2sutils ../copy.img
	@./ext2sutils dup ../copy.img /folder/ostechnix.txt /5112bytefile.txt
	@./ext2sutils dup ../copy.img /deneme.txt /folder/vi_calisma.txt
	@./ext2sutils dup ../copy.img /deneme.txt /birdendokuza.txt

rm: ext2sutils ../copy.img
	@./ext2sutils rm ../copy.img /folder/ostechnix.txt

test: ext2fs.h ext2fs.cpp ext2sutils ../copy.img
	@make all
	@make dup

clear_img:
	@rm -rf ../copy.img
#	@rm -rf ../example.img
#	@rm -rf ../fs-root

prepare_img:
#	dd if=/dev/zero of=../example.img bs=2048 count=512
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