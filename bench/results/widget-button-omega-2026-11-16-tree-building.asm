
build/site-library-targets/profile/jit.bin:     file format binary


Disassembly of section .data:

0000000000007c14 <.data+0x7c14>:
    7c14:	55                   	push   %rbp
    7c15:	48 89 e5             	mov    %rsp,%rbp
    7c18:	48 81 ec d0 05 02 00 	sub    $0x205d0,%rsp
    7c1f:	48 3b 25 d2 0d 38 00 	cmp    0x380dd2(%rip),%rsp        # 0x3889f8
    7c26:	73 0c                	jae    0x7c34
    7c28:	bf 06 00 ff ff       	mov    $0xffff0006,%edi
    7c2d:	e8 88 ad 12 00       	call   0x1329ba
    7c32:	0f 0b                	ud2
    7c34:	48 89 5d f8          	mov    %rbx,-0x8(%rbp)
    7c38:	48 83 2d a8 0d 38 00 	subq   $0x1,0x380da8(%rip)        # 0x3889e8
    7c3f:	01 
    7c40:	73 0c                	jae    0x7c4e
    7c42:	bf 04 00 ff ff       	mov    $0xffff0004,%edi
    7c47:	e8 6e ad 12 00       	call   0x1329ba
    7c4c:	0f 0b                	ud2
    7c4e:	48 8d 1d eb 2a 13 00 	lea    0x132aeb(%rip),%rbx        # 0x13a740
    7c55:	4c 8b 5d 10          	mov    0x10(%rbp),%r11
    7c59:	4c 89 1c 24          	mov    %r11,(%rsp)
    7c5d:	44 8b 5d 18          	mov    0x18(%rbp),%r11d
    7c61:	44 89 5c 24 08       	mov    %r11d,0x8(%rsp)
    7c66:	e8 e4 a7 ff ff       	call   0x244f
    7c6b:	4c 69 d8 ab 05 02 00 	imul   $0x205ab,%rax,%r11
    7c72:	4a 8d 04 1b          	lea    (%rbx,%r11,1),%rax
    7c76:	49 c7 c3 60 fa fd ff 	mov    $0xfffffffffffdfa60,%r11
    7c7d:	46 0f 10 bc 18 a0 05 	movups 0x205a0(%rax,%r11,1),%xmm15
    7c84:	02 00 
    7c86:	46 0f 11 7c 1d e8    	movups %xmm15,-0x18(%rbp,%r11,1)
    7c8c:	49 83 eb f0          	sub    $0xfffffffffffffff0,%r11
    7c90:	0f 85 e7 ff ff ff    	jne    0x7c7d
    7c96:	4c 8b 98 a0 05 02 00 	mov    0x205a0(%rax),%r11
    7c9d:	4c 89 5d e8          	mov    %r11,-0x18(%rbp)
    7ca1:	44 0f b7 98 a8 05 02 	movzwl 0x205a8(%rax),%r11d
    7ca8:	00 
    7ca9:	66 44 89 5d f0       	mov    %r11w,-0x10(%rbp)
    7cae:	44 0f b6 98 aa 05 02 	movzbl 0x205aa(%rax),%r11d
    7cb5:	00 
    7cb6:	44 88 5d f2          	mov    %r11b,-0xe(%rbp)
    7cba:	0f b6 45 d2          	movzbl -0x2e(%rbp),%eax
    7cbe:	48 8b 5d f8          	mov    -0x8(%rbp),%rbx
    7cc2:	c9                   	leave
    7cc3:	c3                   	ret
