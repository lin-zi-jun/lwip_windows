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
;* 函数名称 : uMEM_Init
;* 描    述 : 内存管理初始化
;* 输　	 入 : Addr: 有效的内存块的起始地址, Size: 内存块的大小
;*   
;* 输　	 出 : TRUE OR FALSE
;*----------------------------------------------------------------------------------------------------
;* 修改备注 : 
;*****************************************************************************************************
;*/
uint8 uMEM_Init(void)
{
    SYS_ARCH_DECL_PROTECT(x);
	FreeMem *ThisFreeMem = NULL;
    void *Addr = umem;
	uint32 Size = U_MEM_SZIE;
	
	SYS_ARCH_PROTECT(x);

	/* 调整内存块的大小, 使其为字的倍数 */
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
;* 函数名称 : uMEM_New
;* 描    述 : 内存申请
;* 输　	 入 : Size: 内存块的大小
;*   
;* 输　	 出 : 可用的内存块的起始地址 或 NULL
;*----------------------------------------------------------------------------------------------------
;* 修改备注 : 
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
	
	/* 调整分配的大小,按字分配,比要求的大,并加上占用内存头 */
	Size = ((Size + (sizeof(int) - 1)) & ~(sizeof(int) - 1)) + sizeof(UsingMem);

	/*when mem free, we need to place a FreeMem at the freed addr, we should sure addr is large enough*/
	if(Size < sizeof(FreeMem))
    {
        Size = sizeof(FreeMem);
	}

	/* 查找第一块足够大的空闲内存 */
	ThisFreeMem = uMEM_FreeMem;

	while (ThisFreeMem != NULL)
	{
		if (ThisFreeMem->Size >= Size)
		{
			break;
		}

		ThisFreeMem = ThisFreeMem->Next;
	}

	/* 没有一块足够大的空闲内存可分配 */
	if (ThisFreeMem == NULL)
	{
		SYS_ARCH_UNPROTECT(x);
		
		return NULL;
	}

	/* 如果剩余的空间不足以形成一个空闲内存头, 则整块空闲内存分配出去 */
	if (ThisFreeMem->Size < (Size + sizeof(FreeMem)))
	{
		/* 从双向链表删除该节点 */
		if (ThisFreeMem->Prev)
		{
			ThisFreeMem->Prev->Next = ThisFreeMem->Next;
		}

		if (ThisFreeMem->Next)
		{
			ThisFreeMem->Next->Prev = ThisFreeMem->Prev;
		}

		/* 调整分配的大小 */
		Size = ThisFreeMem->Size;

		Rt = (UsingMem *)ThisFreeMem;
	}
	else
	{
		/* 调整未分配的大小 */
		ThisFreeMem->Size -= Size;

		/* 从该空闲内存块的高端分配 */
		Rt = (UsingMem *)((uint8 *)ThisFreeMem + ThisFreeMem->Size);
	}

	/* 占用内存的大小 */
	Rt->Size = Size;
	
	SYS_ARCH_UNPROTECT(x);
	
	/* 用户可使用的内存起始地址 */
	return (uint8 *)Rt + sizeof(UsingMem);
}


/*
;*****************************************************************************************************
;* 函数名称 : uMEM_Free
;* 描    述 : 内存释放
;* 输　	 入 : Addr: 已分配的内存块的起始地址
;*   
;* 输　	 出 : 无
;*----------------------------------------------------------------------------------------------------
;* 修改备注 : 
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
	
	/* 计算占用内存头 */
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

	/* 如果两块内存相邻 */
	if ( ((uint8 *)ThisFreeMem + ThisFreeMem->Size) == (uint8 *)ThisUsingMem )
	{
		/* 合并之 */
		ThisFreeMem->Size += Size;

		/* 有下一块 */
		if (ThisFreeMem->Next)
		{
			/* 下一块 */
			Temp = ThisFreeMem->Next;

			/* 如果两块内存相邻 */
			if ((uint8 *)ThisUsingMem + Size == (uint8 *)(Temp))
			{
				/* 合并之 */
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

		/* 有下一块 */
		if (ThisFreeMem->Next)
		{	
			/* 下一块 */
			Temp = ThisFreeMem->Next;

			ThisFreeMem->Next = (FreeMem *)ThisUsingMem;

			/* 如果两块内存相邻 */
			if ((uint8 *)ThisUsingMem + Size == (uint8 *)(Temp))
			{
				/* 合并之 */
				((FreeMem *)ThisUsingMem)->Next = Temp->Next;

				if (Temp->Next)
				{
					Temp->Next->Prev = (FreeMem *)ThisUsingMem;
				}

				((FreeMem *)ThisUsingMem)->Size = Size + Temp->Size;
			}
			else
			{
				/* 插入一个节点 */
				((FreeMem *)ThisUsingMem)->Next = Temp;

				((FreeMem *)ThisUsingMem)->Size = Size;

				Temp->Prev = (FreeMem *)ThisUsingMem;
			}
		}
		else
		{	
			/* 插入一个节点 */
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

