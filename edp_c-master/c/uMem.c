/*
 A minimum implementation of memory manage, This is important for Embeded systems, since we can not use C lib Freely.
 This implementation is something similar to JiaoJinXing's, I just port it to LwIP.

 By Forrest.
*/

//#include "lwip/arch.h"
#include "lwip/sys.h"
#include "uMEM.h"

#define NULL  0
#define TRUE  1
#define FALSE 0

#define U_MEM_SZIE (7*1024)
static uint8 umem[U_MEM_SZIE];

FreeMem *uMEM_FreeMem = NULL;

/*
;*****************************************************************************************************
;* �������� : uMEM_Init
;* ��    �� : �ڴ�����ʼ��
;* �䡡	 �� : Addr: ��Ч���ڴ�����ʼ��ַ, Size: �ڴ��Ĵ�С
;*   
;* �䡡	 �� : TRUE OR FALSE
;*----------------------------------------------------------------------------------------------------
;* �޸ı�ע : 
;*****************************************************************************************************
;*/
uint8 uMEM_Init(void)
{
    SYS_ARCH_DECL_PROTECT(x);
	FreeMem *ThisFreeMem = NULL;
    void *Addr = umem;
	uint32 Size = U_MEM_SZIE;
	
	SYS_ARCH_PROTECT(x);

	/* �����ڴ��Ĵ�С, ʹ��Ϊ�ֵı��� */
	Size = Size & ~(sizeof(int) - 1);

	if (Addr != NULL && Size > sizeof(UsingMem))
	{
		ThisFreeMem = (FreeMem *)Addr;

		ThisFreeMem->Prev = NULL;
		ThisFreeMem->Next = NULL;
		ThisFreeMem->Size = Size;

		uMEM_FreeMem = ThisFreeMem;

		SYS_ARCH_UNPROTECT(x);
		
		return TRUE;
	}

	SYS_ARCH_UNPROTECT(x);
		
	return FALSE;
}


/*
;*****************************************************************************************************
;* �������� : uMEM_New
;* ��    �� : �ڴ�����
;* �䡡	 �� : Size: �ڴ��Ĵ�С
;*   
;* �䡡	 �� : ���õ��ڴ�����ʼ��ַ �� NULL
;*----------------------------------------------------------------------------------------------------
;* �޸ı�ע : 
;*****************************************************************************************************
;*/
void *uMEM_New(uint32 Size)
{
    SYS_ARCH_DECL_PROTECT(x);

	FreeMem *ThisFreeMem = NULL;
	UsingMem *Rt = NULL;

	if(Size > U_MEM_SZIE)
	{
        return NULL;
	}
	
	SYS_ARCH_PROTECT(x);
	
	/* ��������Ĵ�С,���ַ���,��Ҫ��Ĵ�,������ռ���ڴ�ͷ */
	Size = ((Size + (sizeof(int) - 1)) & ~(sizeof(int) - 1)) + sizeof(UsingMem);

	/*when mem free, we need to place a FreeMem at the freed addr, we should sure addr is large enough*/
	if(Size < sizeof(FreeMem))
    {
        Size = sizeof(FreeMem);
	}

	/* ���ҵ�һ���㹻��Ŀ����ڴ� */
	ThisFreeMem = uMEM_FreeMem;

	while (ThisFreeMem != NULL)
	{
		if (ThisFreeMem->Size >= Size)
		{
			break;
		}

		ThisFreeMem = ThisFreeMem->Next;
	}

	/* û��һ���㹻��Ŀ����ڴ�ɷ��� */
	if (ThisFreeMem == NULL)
	{
		SYS_ARCH_UNPROTECT(x);
		
		return NULL;
	}

	/* ���ʣ��Ŀռ䲻�����γ�һ�������ڴ�ͷ, ����������ڴ�����ȥ */
	if (ThisFreeMem->Size < (Size + sizeof(FreeMem)))
	{
		/* ��˫������ɾ���ýڵ� */
		if (ThisFreeMem->Prev)
		{
			ThisFreeMem->Prev->Next = ThisFreeMem->Next;
		}

		if (ThisFreeMem->Next)
		{
			ThisFreeMem->Next->Prev = ThisFreeMem->Prev;
		}

		/* ��������Ĵ�С */
		Size = ThisFreeMem->Size;

		Rt = (UsingMem *)ThisFreeMem;
	}
	else
	{
		/* ����δ����Ĵ�С */
		ThisFreeMem->Size -= Size;

		/* �Ӹÿ����ڴ��ĸ߶˷��� */
		Rt = (UsingMem *)((uint8 *)ThisFreeMem + ThisFreeMem->Size);
	}

	/* ռ���ڴ�Ĵ�С */
	Rt->Size = Size;
	
	SYS_ARCH_UNPROTECT(x);
	
	/* �û���ʹ�õ��ڴ���ʼ��ַ */
	return (uint8 *)Rt + sizeof(UsingMem);
}


/*
;*****************************************************************************************************
;* �������� : uMEM_Free
;* ��    �� : �ڴ��ͷ�
;* �䡡	 �� : Addr: �ѷ�����ڴ�����ʼ��ַ
;*   
;* �䡡	 �� : ��
;*----------------------------------------------------------------------------------------------------
;* �޸ı�ע : 
;*****************************************************************************************************
;*/
void uMEM_Free(void *Addr)
{
	SYS_ARCH_DECL_PROTECT(x);

	FreeMem *ThisFreeMem = NULL, *Temp = NULL;
	UsingMem *ThisUsingMem = NULL;
	uint32 Size;
    
	if (Addr == NULL)
	{
		return ;
	}
	/*check more about Addr*/
    if((uint8 *)Addr < (uint8 *)uMEM_FreeMem)
    {
        return;
	}

	if((uint8 *)Addr > (uint8 *)uMEM_FreeMem + U_MEM_SZIE)
    {
        return;
	}
 
	SYS_ARCH_PROTECT(x);
	
	/* ����ռ���ڴ�ͷ */
	ThisUsingMem = (UsingMem *)((uint8 *)Addr - sizeof(UsingMem));
    ThisFreeMem = uMEM_FreeMem;
	
	while (1)
	{
		if (ThisFreeMem > (FreeMem *)ThisUsingMem)
		{		
			ThisFreeMem = ThisFreeMem->Prev;
			break;
		}

		if (ThisFreeMem->Next == NULL)
		{
			break;
		}
		
		ThisFreeMem = ThisFreeMem->Next;
	}

	Size = ThisUsingMem->Size;

	/* ��������ڴ����� */
	if ( ((uint8 *)ThisFreeMem + ThisFreeMem->Size) == (uint8 *)ThisUsingMem )
	{
		/* �ϲ�֮ */
		ThisFreeMem->Size += Size;

		/* ����һ�� */
		if (ThisFreeMem->Next)
		{
			/* ��һ�� */
			Temp = ThisFreeMem->Next;

			/* ��������ڴ����� */
			if ((uint8 *)ThisUsingMem + Size == (uint8 *)(Temp))
			{
				/* �ϲ�֮ */
				ThisFreeMem->Next = Temp->Next;

				ThisFreeMem->Size += Temp->Size;

				if (Temp->Next)
				{
					Temp->Next->Prev = ThisFreeMem;
				}
			}
		}
	}
	else
	{
		((FreeMem *)ThisUsingMem)->Prev = ThisFreeMem;

		/* ����һ�� */
		if (ThisFreeMem->Next)
		{	
			/* ��һ�� */
			Temp = ThisFreeMem->Next;

			ThisFreeMem->Next = (FreeMem *)ThisUsingMem;

			/* ��������ڴ����� */
			if ((uint8 *)ThisUsingMem + Size == (uint8 *)(Temp))
			{
				/* �ϲ�֮ */
				((FreeMem *)ThisUsingMem)->Next = Temp->Next;

				if (Temp->Next)
				{
					Temp->Next->Prev = (FreeMem *)ThisUsingMem;
				}

				((FreeMem *)ThisUsingMem)->Size = Size + Temp->Size;
			}
			else
			{
				/* ����һ���ڵ� */
				((FreeMem *)ThisUsingMem)->Next = Temp;

				((FreeMem *)ThisUsingMem)->Size = Size;

				Temp->Prev = (FreeMem *)ThisUsingMem;
			}
		}
		else
		{	
			/* ����һ���ڵ� */
			ThisFreeMem->Next = (FreeMem *)ThisUsingMem;
			
			((FreeMem *)ThisUsingMem)->Next = NULL;

			((FreeMem *)ThisUsingMem)->Size = Size;
		}
	}
	
	SYS_ARCH_UNPROTECT(x);
}

/*
;*****************************************************************************************************
;*											End of File
;*****************************************************************************************************
;*/	

