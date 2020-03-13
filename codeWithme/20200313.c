/*
测开面试题
输入 Tencent is one of the greatest company of the world
要求 输出 去除元音字母 aeiou AEIOU
*/
#include <stdio.h>
#include <stdlib.h>

int main()
{
char c;
int n=1;
int j=0;
char * s=(char*)malloc(n*sizeof(char));
while(c=getchar())
{
if (c=='\n')
{
break;return 0;
}
if(c!='a'&&c!='e'&&c!='i'&&c!='o'&&c!='u')
{s[j++]=c;
}
if(j>=n)
{char * t=(char *)realloc(s,(++n));
if (t==NULL)
{
printf("sorry");
}
}


}
printf("%s",s);
}

