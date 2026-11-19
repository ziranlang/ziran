## Vectors · 1000 (493 samples)

| share | where |
|---:|---|
| 79.3 % | native image |
| 7.7 % | libc.so.6:__default_morecore@GLIBC_2.2.5 |
| 6.6 % | libc.so.6:_dl_mcount_wrapper@GLIBC_2.2.5 |
| 3.8 % | native-runner-after:NativeResize |
| 1.2 % | libc.so.6:realloc@@GLIBC_2.2.5 |
| 0.4 % | native-runner-after:native_release |
| 0.4 % | libc.so.6:__libc_free@@GLIBC_2.2.5 |
| 0.2 % | libc.so.6:? |
| 0.2 % | libc.so.6:timer_settime@GLIBC_2.2.5 |

Hottest 64-byte native image ranges (offset from the mapping):

- 0x180 · 171 samples
- 0x100 · 119 samples
- 0x1c0 · 55 samples
- 0x340 · 31 samples
- 0x1700 · 4 samples
- 0x300 · 4 samples
- 0x200 · 2 samples
- 0x280 · 1 samples

```
Vectors (samples 171 in the 64 bytes at 0x180)
     160:	add    %ch,%al
     162:	(bad)
     163:	(bad)
     164:	add    %al,(%rax)
     166:	sub    $0x1,%r15
     16a:	jb     0x397
     170:	cmp    $0x13,%rbx
     174:	jle    0x316
     17a:	jmp    0x325
     17f:	nop
     180:	mov    %rsi,%rdx
     183:	imul   %rbx,%rdx
     187:	cmp    %rcx,%rax
     18a:	jge    0x32d
     190:	nopw   0x0(%rax,%rax,1)
     199:	nopl   0x0(%rax)
     1a0:	lea    0x1(%rax),%r8
     1a4:	mov    %rdx,(%rdi,%rax,8)
     1a8:	mov    $0x1,%eax
     1ad:	mov    %rcx,%rdx
     1b0:	mov    %r8,%rcx
     1b3:	nopw   0x0(%rax,%rax,1)
     1bc:	nopl   0x0(%rax)
     1c0:	add    $0x1,%rsi
     1c4:	sub    $0x1,%r15
     1c8:	jb     0x3aa
     1ce:	cmp    $0x3e7,%rsi
     1d5:	jle    0x34c
     1db:	nopl   0x0(%rax,%rax,1)
```

```
Vectors (samples 119 in the 64 bytes at 0x100)
      e0:	sub    $0x1,%r15
      e4:	jb     0xc7
      ea:	cmp    $0x3e7,%rax
      f1:	jg     0x2fb
      f7:	nopw   0x0(%rax,%rax,1)
     100:	mov    (%rdi,%rax,8),%r8
     104:	add    %r8,%rdx
     107:	add    $0x1,%rax
     10b:	sub    $0x1,%r15
     10f:	jb     0x384
     115:	nopw   0x0(%rax,%rax,1)
     11e:	xchg   %ax,%ax
     120:	cmp    $0x3e7,%rax
     127:	jle    0x100
     12d:	jmp    0x30b
     132:	nopw   0x0(%rax,%rax,1)
     13b:	nopl   0x0(%rax,%rax,1)
     140:	xor    %r13d,%r13d
     143:	mov    %r13,%rdx
     146:	mov    $0x8,%ecx
     14b:	call   0x187d
     150:	xor    %edi,%edi
     152:	add    $0x1,%rbx
     156:	mov    %r13,%rsi
     159:	mov    %r13,%rdx
     15c:	mov    $0x8,%ecx
```

```
Vectors (samples 55 in the 64 bytes at 0x1c0)
     1a0:	lea    0x1(%rax),%r8
     1a4:	mov    %rdx,(%rdi,%rax,8)
     1a8:	mov    $0x1,%eax
     1ad:	mov    %rcx,%rdx
     1b0:	mov    %r8,%rcx
     1b3:	nopw   0x0(%rax,%rax,1)
     1bc:	nopl   0x0(%rax)
     1c0:	add    $0x1,%rsi
     1c4:	sub    $0x1,%r15
     1c8:	jb     0x3aa
     1ce:	cmp    $0x3e7,%rsi
     1d5:	jle    0x34c
     1db:	nopl   0x0(%rax,%rax,1)
     1e0:	jmp    0x357
     1e5:	nopw   0x0(%rax,%rax,1)
     1ee:	nopw   0x0(%rax,%rax,1)
     1f7:	nopw   0x0(%rax,%rax,1)
     200:	cmp    $0x0,%r9
     204:	sete   %al
     207:	movzbl %al,%eax
     20a:	shl    $0x3,%rax
     20e:	lea    (%r9,%r9,1),%rdx
     212:	lea    (%rdx,%rax,1),%r10
     216:	mov    %rcx,%rdi
     219:	mov    %r9,%rsi
     21c:	mov    %r10,%rdx
     21f:	mov    %rcx,-0x50(%rbp)
```
