
build/site-library-targets/profile/jit.bin:     file format binary


Disassembly of section .data:

0000000000006972 <.data+0x6972>:
    6972:	55                   	push   %rbp
    6973:	48 89 e5             	mov    %rsp,%rbp
    6976:	48 83 ec 20          	sub    $0x20,%rsp
    697a:	48 3b 25 77 c0 37 00 	cmp    0x37c077(%rip),%rsp        # 0x3829f8
    6981:	73 0c                	jae    0x698f
    6983:	bf 06 00 ff ff       	mov    $0xffff0006,%edi
    6988:	e8 24 5a 12 00       	call   0x12c3b1
    698d:	0f 0b                	ud2
    698f:	48 89 5d f8          	mov    %rbx,-0x8(%rbp)
    6993:	48 83 2d 4d c0 37 00 	subq   $0x1,0x37c04d(%rip)        # 0x3829e8
    699a:	01 
    699b:	73 0c                	jae    0x69a9
    699d:	bf 04 00 ff ff       	mov    $0xffff0004,%edi
    69a2:	e8 0a 5a 12 00       	call   0x12c3b1
    69a7:	0f 0b                	ud2
    69a9:	48 8d 1d 90 dd 12 00 	lea    0x12dd90(%rip),%rbx        # 0x134740
    69b0:	4c 8b 5d 10          	mov    0x10(%rbp),%r11
    69b4:	4c 89 1c 24          	mov    %r11,(%rsp)
    69b8:	44 8b 5d 18          	mov    0x18(%rbp),%r11d
    69bc:	44 89 5c 24 08       	mov    %r11d,0x8(%rsp)
    69c1:	e8 08 b4 ff ff       	call   0x1dce
    69c6:	4c 69 d8 ab 05 02 00 	imul   $0x205ab,%rax,%r11
    69cd:	4a 8d 04 1b          	lea    (%rbx,%r11,1),%rax
    69d1:	0f b6 80 8a 05 02 00 	movzbl 0x2058a(%rax),%eax
    69d8:	48 8b 5d f8          	mov    -0x8(%rbp),%rbx
    69dc:	c9                   	leave
    69dd:	c3                   	ret
