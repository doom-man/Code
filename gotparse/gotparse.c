#include <stdio.h>
#include <sys/procfs.h>
#include <stdlib.h>       
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <elf.h>
#include <string.h>
#define PATH  "./gotparse"
#define LOGD(x ...) printf(x)
int main(void){
	int fd;
	fd = open(PATH , O_RDONLY );
	printf("[+] sizeof(Elf32_Ehdr) 0x%x \n",sizeof(Elf32_Ehdr));
	Elf32_Ehdr *ehdr = (Elf32_Ehdr *)malloc(sizeof(Elf32_Ehdr));
	if( read(fd, ehdr, sizeof(Elf32_Ehdr)) != sizeof(Elf32_Ehdr) ){
		printf("[-] read elf 文件头失败 \n ");
		return -1;
	}
	uint32_t shdr_base = ehdr -> e_shoff ; // 节头表偏移
	uint16_t shnum = ehdr -> e_shnum ; // 节头表项数
	printf("[+] 节头表偏移 0x%x\n",shdr_base);
	printf("[+] print sizeof section header 0x%x \n",sizeof(Elf32_Shdr));
	printf("[+] header name section idx 0x%x \n",ehdr->e_shstrndx);
	if (ehdr -> e_shstrndx==SHN_UNDEF){
		printf("[-] do not have shstrndx\n");
		return -1;
	}

	uint32_t shstr_base = shdr_base + ehdr -> e_shstrndx * sizeof(Elf32_Shdr); // shstrtab地址
	printf("[+] shdrstrndx addresss 0x%x \n" , shstr_base);
	Elf32_Shdr *shstr = (Elf32_Shdr *)malloc(sizeof(Elf32_Shdr));
	lseek(fd, shstr_base, SEEK_SET);
	//缓冲header of  shstrtab
	read(fd, shstr, sizeof(Elf32_Shdr)); 

	//读取shstrtab表
	char *shstrtab = (char *)malloc(shstr -> sh_size);
	printf("[+] the offset of sectino 0x%x\n",shstr -> sh_offset);
	lseek(fd, shstr -> sh_offset, SEEK_SET);
	read(fd, shstrtab, shstr -> sh_size);
	
	Elf32_Shdr *shdr = (Elf32_Shdr *)malloc(sizeof(Elf32_Shdr));
	lseek(fd, shdr_base, SEEK_SET);
	uint16_t i;
	char *str = NULL;
	Elf32_Shdr *relplt_shdr = (Elf32_Shdr *) malloc(sizeof(Elf32_Shdr));
	Elf32_Shdr *dynsym_shdr = (Elf32_Shdr *) malloc(sizeof(Elf32_Shdr));
	Elf32_Shdr *dynstr_shdr = (Elf32_Shdr *) malloc(sizeof(Elf32_Shdr));
	Elf32_Shdr *got_shdr = (Elf32_Shdr *) malloc(sizeof(Elf32_Shdr));
	//遍历每个section header
	for(i = 0; i < shnum; ++i) {
		read(fd, shdr, sizeof(Elf32_Shdr));
		str = shstrtab + shdr -> sh_name;
		if(strcmp(str, ".dynsym") == 0)
			memcpy(dynsym_shdr, shdr, sizeof(Elf32_Shdr));
		else if(strcmp(str, ".dynstr") == 0)
			memcpy(dynstr_shdr, shdr, sizeof(Elf32_Shdr));
		else if(strcmp(str, ".got") == 0)
			memcpy(got_shdr, shdr, sizeof(Elf32_Shdr));
		else if(strcmp(str, ".rel.plt") == 0)
			memcpy(relplt_shdr, shdr, sizeof(Elf32_Shdr));
	}

	//读取字符表
	char *dynstr = (char *) malloc(sizeof(char) * dynstr_shdr->sh_size);
	lseek(fd, dynstr_shdr->sh_offset, SEEK_SET);
	if(read(fd, dynstr, dynstr_shdr->sh_size) != dynstr_shdr->sh_size)
		return -1;
	
	//读取符号表
	Elf32_Sym *dynsymtab = (Elf32_Sym *) malloc(dynsym_shdr->sh_size);
	printf("dynsym_shdr->sh_size\t0x%x\n", dynsym_shdr->sh_size);
	lseek(fd, dynsym_shdr->sh_offset, SEEK_SET);
	if(read(fd, dynsymtab, dynsym_shdr->sh_size) != dynsym_shdr->sh_size)
		return -1;
	
	//读取重定位表
	Elf32_Rel *rel_ent = (Elf32_Rel *) malloc(sizeof(Elf32_Rel));
	lseek(fd, relplt_shdr->sh_offset, SEEK_SET);
	if(read(fd, rel_ent, sizeof(Elf32_Rel)) != sizeof(Elf32_Rel))
		return -1;
	
	char symbol_name[] = "puts";
	int offset = 0 ;
	printf("[+] print sizeof elf32_rel 0x%x\n",sizeof(Elf32_Rel));
	for (i = 0; i < relplt_shdr->sh_size / sizeof(Elf32_Rel); i++)
	{
		//r_info 指定重定位符号表索引，r_offse 指定重定位操作的位置。
		uint16_t ndx = ELF32_R_SYM(rel_ent->r_info);
		LOGD("ndx = %d, str = %s\n", ndx, dynstr + dynsymtab[ndx].st_name);
		if (strcmp(dynstr + dynsymtab[ndx].st_name, symbol_name) == 0)
		{
			LOGD("符号%s在got表的偏移地址为: 0x%x\n", symbol_name, rel_ent->r_offset);
			offset = rel_ent->r_offset;
			break;
		}
		if(read(fd, rel_ent, sizeof(Elf32_Rel)) != sizeof(Elf32_Rel))
		{
			LOGD("获取符号%s的重定位信息失败\n", symbol_name);
			return -1;
		}
	}
	//获取指定符号的地址
	if(offset == 0)
	{
		LOGD("获取符号%s在got表中的偏移地址失败，可能为静态链接，开始重新获取符号地址", symbol_name);
		for(i = 0; i < (dynsym_shdr->sh_size) / sizeof(Elf32_Sym); ++i)
		{
			if(strcmp(dynstr + dynsymtab[i].st_name, symbol_name) == 0)
			{
				LOGD("符号%s的地址位: 0x%x\n", symbol_name, dynsymtab[i].st_value);
				offset = dynsymtab[i].st_value;
				break;
			}
		}
	}
	if(offset == 0)
	{
		LOGD("符号%s地址获取失败\n", symbol_name);
		return -1;
	}
	// 成功解析got偏移
	
	return 0;
}
