/* Scalar, single-thread reference algorithms; build with cc -O2 (no LTO). */
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
typedef int64_t I;
I Fib(I n) {
    return n<2?n:Fib(n-1)+Fib(n-2);
}
I Queens(I mask,I cols,I left,I right) {
    if(cols==mask)return 1;
    I available=mask&~(cols|left|right),total=0;
    while(available) {
        I bit=available&-available;
        available-=bit;
        total+=Queens(mask,cols|bit,(left|bit)<<1,(right|bit)>>1);
    }
    return total;
}
I Matrix(I n) {
    I *a=calloc(n*n,sizeof(I)),*b=calloc(n*n,sizeof(I)),*c=calloc(n*n,sizeof(I)),sum=0;
    for(I i=0;i<n*n;i++) {
        a[i]=i%17;
        b[i]=i%13;
    }
    for(I i=0;i<n;i++)for(I k=0;k<n;k++)for(I j=0;j<n;j++)c[i*n+j]+=a[i*n+k]*b[k*n+j];
    for(I i=0;i<n*n;i++)sum+=c[i];
    free(a);
    free(b);
    free(c);
    return sum;
}
void Quick(I *a,I lo,I hi) {
    I i=lo,j=hi,pivot=a[(lo+hi)/2];
    while(i<=j) {
        while(a[i]<pivot)i++;
        while(a[j]>pivot)j--;
        if(i<=j) {
            I t=a[i];
            a[i]=a[j];
            a[j]=t;
            i++;
            j--;
        }
    }
    if(lo<j)Quick(a,lo,j);
    if(i<hi)Quick(a,i,hi);
}
I Sort(I n) {
    I *a=calloc(n,sizeof(I)),seed=42,hash=0;
    for(I i=0;i<n;i++) {
        seed=(seed*1664525+1013904223)&2147483647;
        a[i]=seed;
    }
    Quick(a,0,n-1);
    for(I i=0;i<n;i++)hash=(hash*31+a[i])%1000000007;
    free(a);
    return hash;
}
I Hash(I n) {
    const char *text="the quick brown fox jumps over the lazy dog, again and again";
    uint64_t hash=1469598103934665603ULL;
    for(I r=0;r<n;r++)for(size_t i=0;i<strlen(text);i++)hash=(hash^(uint64_t)text[i])*1099511628211ULL;
    return hash%1000000007;
}
double A(I i,I j) {
    return 1.0/(double)((i+j)*(i+j+1)/2+i+1);
}
void Multiply(double *u,double *v,I n,int transpose) {
    for(I i=0;i<n;i++) {
        double sum=0;
        for(I j=0;j<n;j++)sum+=u[j]*(transpose?A(j,i):A(i,j));
        v[i]=sum;
    }
}
I Spectral(I n) {
    double *u=calloc(n,sizeof(double)),*v=calloc(n,sizeof(double)),*tmp=calloc(n,sizeof(double));
    for(I i=0;i<n;i++)u[i]=1;
    for(I r=0;r<10;r++) {
        Multiply(u,tmp,n,0);
        Multiply(tmp,v,n,1);
        Multiply(v,tmp,n,0);
        Multiply(tmp,u,n,1);
    }
    double uv=0,vv=0;
    for(I i=0;i<n;i++) {
        uv+=u[i]*v[i];
        vv+=v[i]*v[i];
    }
    I result=(I)(sqrt(uv/vv)*1e9+0.5);
    free(u);
    free(v);
    free(tmp);
    return result;
}
I Mandelbrot(I n) {
    I hash=0;
    for(I y=0;y<n;y++) {
        I byte=0,bits=0;
        for(I x=0;x<n;x++) {
            double cr=2.0*x/n-1.5,ci=2.0*y/n-1.0,zr=0,zi=0,tr=0,ti=0;
            I iter=0;
            while(iter<50&&tr+ti<=4) {
                zi=2*zr*zi+ci;
                zr=tr-ti+cr;
                tr=zr*zr;
                ti=zi*zi;
                iter++;
            }
            byte=byte*2+(tr+ti<=4);
            bits++;
            if(bits==8) {
                hash=(hash*31+byte)%1000000007;
                byte=bits=0;
            }
        }
        if(bits)hash=(hash*31+(byte<<(8-bits)))%1000000007;
    }
    return hash;
}
I Fannkuch(I n) {
    I perm[16]= {
        0
    }
    ,work[16]= {
        0
    }
    ,count[16]= {
        0
    }
    ;
    for(I i=0;i<n;i++)perm[i]=i;
    I r=n,index=0,checksum=0,maximum=0;
    for(;;) {
        while(r!=1) {
            count[r-1]=r;
            r--;
        }
        for(I i=0;i<n;i++)work[i]=perm[i];
        I flips=0;
        while(work[0]) {
            I k=work[0],i=0;
            while(i<k) {
                I t=work[i];
                work[i]=work[k];
                work[k]=t;
                i++;
                k--;
            }
            flips++;
        }
        if(flips>maximum)maximum=flips;
        checksum+=index%2==0?flips:-flips;
        for(;;) {
            if(r==n)return checksum*1000+maximum;
            I first=perm[0];
            for(I i=0;i<r;i++)perm[i]=perm[i+1];
            perm[r]=first;
            count[r]--;
            if(count[r]>0)break;
            r++;
        }
        index++;
    }
}
typedef struct Node {
    struct Node *left,*right;
}
Node;
Node *Tree(I depth) {
    Node *node=calloc(1,sizeof(Node));
    if(depth>0) {
        node->left=Tree(depth-1);
        node->right=Tree(depth-1);
    }
    return node;
}
I TreeCheck(Node *node) {
    return node->left?1+TreeCheck(node->left)+TreeCheck(node->right):1;
}
void TreeFree(Node *node) {
    if(node->left) {
        TreeFree(node->left);
        TreeFree(node->right);
    }
    free(node);
}
I Trees(I n) {
    Node *stretch=Tree(n+1);
    I sum=TreeCheck(stretch);
    TreeFree(stretch);
    Node *longlived=Tree(n);
    for(I depth=4;depth<=n;depth+=2) {
        I iterations=(I)1<<(n-depth+4);
        for(I i=0;i<iterations;i++) {
            Node *tree=Tree(depth);
            sum+=TreeCheck(tree);
            TreeFree(tree);
        }
    }
    sum+=TreeCheck(longlived);
    TreeFree(longlived);
    return sum;
}
typedef struct {
    double x,y,z,vx,vy,vz,mass;
}
Body;
I Nbody(I n) {
    double pi=3.141592653589793,solar=4*pi*pi,year=365.24;
    Body b[5]= {
        {
            0,0,0,0,0,0,solar
        }
        ,
        {
            4.84143144246472090,-1.16032004402742839,-0.103622044471123109,0.00166007664274403694*year,0.00769901118419740425*year,-0.0000690460016972063023*year,0.000954791938424326609*solar
        }
        ,
        {
            8.34336671824457987,4.12479856412430479,-0.403523417114321381,-0.00276742510726862411*year,0.00499852801234917238*year,0.0000230417297573763929*year,0.000285885980666130812*solar
        }
        ,
        {
            12.8943695621391310,-15.1111514016986312,-0.223307578892655734,0.00296460137564761618*year,0.00237847173959480950*year,-0.0000296589568540237556*year,0.0000436624404335156298*solar
        }
        ,
        {
            15.3796971148509165,-25.9193146099879641,0.179258772950371181,0.00268067772490389322*year,0.00162824170038242295*year,-0.0000951592254519715870*year,0.0000515138902046611451*solar
        }
    }
    ;
    for(I i=1;i<5;i++) {
        b[0].vx-=b[i].vx*b[i].mass/solar;
        b[0].vy-=b[i].vy*b[i].mass/solar;
        b[0].vz-=b[i].vz*b[i].mass/solar;
    }
    for(I step=0;step<n;step++) {
        for(I i=0;i<4;i++)for(I j=i+1;j<5;j++) {
            double dx=b[i].x-b[j].x,dy=b[i].y-b[j].y,dz=b[i].z-b[j].z,d2=dx*dx+dy*dy+dz*dz,mag=0.01/(d2*sqrt(d2));
            b[i].vx-=dx*b[j].mass*mag;
            b[i].vy-=dy*b[j].mass*mag;
            b[i].vz-=dz*b[j].mass*mag;
            b[j].vx+=dx*b[i].mass*mag;
            b[j].vy+=dy*b[i].mass*mag;
            b[j].vz+=dz*b[i].mass*mag;
        }
        for(I i=0;i<5;i++) {
            b[i].x+=0.01*b[i].vx;
            b[i].y+=0.01*b[i].vy;
            b[i].z+=0.01*b[i].vz;
        }
    }
    double energy=0;
    for(I i=0;i<5;i++) {
        energy+=0.5*b[i].mass*(b[i].vx*b[i].vx+b[i].vy*b[i].vy+b[i].vz*b[i].vz);
        for(I j=i+1;j<5;j++) {
            double dx=b[i].x-b[j].x,dy=b[i].y-b[j].y,dz=b[i].z-b[j].z;
            energy-=b[i].mass*b[j].mass/sqrt(dx*dx+dy*dy+dz*dz);
        }
    }
    return (I)(energy*1e9-0.5);
}
static const char *ALU="GGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCCAGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAA";
I Fasta(I n) {
    I hash=0,seed=42;
    for(I i=0;i<2*n;i++)hash=(hash*31+ALU[i%strlen(ALU)])%1000000007;
    const char *chars="acgtBDHKMNRSVWY";
    double p[15]= {
        .27,.12,.12,.27,.02,.02,.02,.02,.02,.02,.02,.02,.02,.02,.02
    }
    ,h[4]= {
        .3029549426680,.1979883004921,.1975473066391,.3015094502008
    }
    ;
    for(I phase=0;phase<2;phase++) {
        I length=phase==0?3*n:5*n;
        for(I i=0;i<length;i++) {
            seed=(seed*3877+29573)%139968;
            double random=(double)seed/139968.0,cumulative=0;
            I index=0;
            while(index<14) {
                cumulative+=phase==0?p[index]:h[index];
                if(random<cumulative)break;
                index++;
            }
            hash=(hash*31+chars[index])%1000000007;
        }
    }
    return hash;
}
I Run(I kind,I n) {
    switch(kind) {
        case 0:return Fib(n);
        case 1:return Nbody(n);
        case 2:return Spectral(n);
        case 3:return Mandelbrot(n);
        case 4:return Fannkuch(n);
        case 5:return Trees(n);
        case 6:return Fasta(n);
        case 7:return Queens(((I)1<<n)-1,0,0,0);
        case 8:return Matrix(n);
        case 9:return Sort(n);
        default:return Hash(n);
    }
}
