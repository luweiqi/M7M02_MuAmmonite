/******************************************************************************
Filename    : hyper.c
Author      : pry
Date        : 02/10/2018
Licence     : LGPL v3+; see COPYING for details.
Description : The hypercall implementation file.
******************************************************************************/

/* Includes ******************************************************************/
#include "RME.h"

#define __HDR_DEFS__
#include "Platform/RVM_platform.h"
#include "Init/syssvc.h"
#include "Init/init.h"
#include "Init/hyper.h"
#undef __HDR_DEFS__

#define __HDR_STRUCTS__
#include "Platform/RVM_platform.h"
#include "Init/syssvc.h"
#include "Init/init.h"
#include "Init/hyper.h"
#undef __HDR_STRUCTS__

/* Private include */
#include "Init/hyper.h"

#define __HDR_PUBLIC_MEMBERS__
#include "Platform/RVM_platform.h"
#include "Init/syssvc.h"
#include "Init/init.h"
#undef __HDR_PUBLIC_MEMBERS__
/* End Includes **************************************************************/

/* Begin Function:_RVM_Strncmp ************************************************
Description : Compare the two strings to see if they are equal.
Input       : const s8* Str1 - The first string.
              const s8* Str2 - The second string.
              ptr_t Num - The number of characters to compare.
Output      : None.
Return      : cnt_t - If Str1>Str2, positive; if Str1==Str2, 0, else negative.
******************************************************************************/
ret_t _RVM_Strncmp(const s8* Str1, const s8* Str2, ptr_t Num)
{
    cnt_t Count;

    for(Count=0;Count<Num;Count++)
    {
        if((Str1[Count]=='\0')||(Str1[Count]!=Str2[Count]))
            return Str1[Count]-Str2[Count];
    }
    
    return 0;
}
/* End Function:_RVM_Strncmp *************************************************/

/* Begin Function:_RVM_Set_Rdy ************************************************
Description : Set the virtual machine as ready to schedule.
Input       : struct RVM_Virt* Virt - The virtual machine to put into the runqueue.
Output      : None.
Return      : None.
******************************************************************************/
void _RVM_Set_Rdy(struct RVM_Virt* Virt)
{
    /* Insert this into the corresponding runqueue */
    RVM_List_Ins(&(Virt->Head),RVM_Run[Virt->Image->Prio].Prev,&(RVM_Run[Virt->Image->Prio]));
    /* Set this runlevel as active */
    RVM_Bitmap[Virt->Image->Prio>>RVM_WORD_ORDER]|=((ptr_t)1)<<(Virt->Image->Prio&RVM_WORD_MASK);
}
/* End Function:_RVM_Set_Rdy *************************************************/

/* Begin Function:_RVM_Clr_Rdy ************************************************
Description : Clear the virtual machine from the runqueue.
Input       : struct RVM_Virt* Virt - The virtual machine to clear from the runqueue.
Output      : None.
Return      : None.
******************************************************************************/
void _RVM_Clr_Rdy(struct RVM_Virt* Virt)
{
    /* See if it is the last thread on the priority level */
    if(Virt->Head.Prev==Virt->Head.Next)
        RVM_Bitmap[Virt->Image->Prio>>RVM_WORD_ORDER]&=~(((ptr_t)1)<<(Virt->Image->Prio&RVM_WORD_MASK));
    
    /* Delete this from the corresponding runqueue */
    RVM_List_Del(Virt->Head.Prev,Virt->Head.Next);
}
/* End Function:_RVM_Clr_Rdy *************************************************/

/* Begin Function:_RVM_Get_High_Rdy *******************************************
Description : Get the highest priority ready virtual machine available.
Input       : None.
Output      : None.
Return      : struct RVM_Virt* Virt - The virtual machine. If all virtual machine
                                      have gone asleep, this will return 0.
******************************************************************************/
struct RVM_Virt* _RVM_Get_High_Rdy(void)
{
    cnt_t Count;
    
    /* See which one is ready, and pick it */
    for(Count=RVM_PRIO_BITMAP-1;Count>=0;Count--)
    {
        if(RVM_Bitmap[Count]==0)
            continue;
        
        Count=_RVM_MSB_Get(RVM_Bitmap[Count])+(Count<<RVM_WORD_ORDER);
        return (struct RVM_Virt*)(RVM_Run[Count].Next);
    }
    
    return 0;
}
/* End Function:_RVM_Get_High_Rdy ********************************************/

/* Begin Function:_RVM_Virt_Switch ********************************************
Description : Switch between two virtual machines.
Input       : struct RVM_Virt* From - The source virtual machine.
              struct RVM_Virt* To - The destination virtual machine.
Output      : None.
Return      : None.
******************************************************************************/
void _RVM_Virt_Switch(struct RVM_Virt* From, struct RVM_Virt* To)
{
    if(From==To)
        return;
    
    /* Just change the thread's priorities */
    if(From!=0)
    {
        RVM_ASSERT(RVM_Thd_Sched_Prio(From->Cap.User_Thd, RVM_WAIT_PRIO)==0);
        RVM_ASSERT(RVM_Thd_Sched_Prio(From->Cap.Int_Thd, RVM_WAIT_PRIO)==0);
    }
    
    if(To!=0)
    {
        RVM_ASSERT(RVM_Thd_Sched_Prio(To->Cap.User_Thd, RVM_USER_PRIO)==0);
        /* If the booting is not done yet, place the interrupt vector at the waiting level as always */
        if((To->Sched.State&RVM_VM_BOOTDONE)==0)
            RVM_ASSERT(RVM_Thd_Sched_Prio(To->Cap.Int_Thd, RVM_WAIT_PRIO)==0);
        else
            RVM_ASSERT(RVM_Thd_Sched_Prio(To->Cap.Int_Thd, RVM_VINT_PRIO)==0);
    }
}
/* End Function:_RVM_Virt_Switch *********************************************/

/* Begin Function:_RVM_Check_Int_Pend *****************************************
Description : Check if there is one pending interrupt for the virtual machine.
Input       : struct RVM_Virt* Virt - The virtual machine to check.
Output      : None.
Return      : If there is one interrupt pending, return 1; else 0.
******************************************************************************/
ret_t _RVM_Check_Int_Pend(struct RVM_Virt* Virt)
{
    cnt_t Count;
    for(Count=0;Count<RVM_VECT_BITMAP;Count++)
    {
        if(Virt->Image->Int_Flags->Flags[Count]!=0)
            return 1;
    }
    return 0;
}
/* End Function:_RVM_Check_Int_Pend ******************************************/

/* Begin Function:_RVM_Set_Int_Flag *******************************************
Description : Set an interrupt's flag for the virtual machine.
Input       : struct RVM_Virt* Virt - The virtual machine to set flag for.
              ptr_t Int_Num - The interrupt number to set flag for.
Output      : None.
Return      : None.
******************************************************************************/
void _RVM_Set_Int_Flag(struct RVM_Virt* Virt, ptr_t Int_Num)
{
    Virt->Image->Int_Flags->Flags[Int_Num>>RVM_WORD_ORDER]|=((ptr_t)1)<<(Int_Num&RVM_WORD_MASK);
}
/* End Function:_RVM_Set_Int_Flag ********************************************/

/* Begin Function:_RVM_Clr_Int_Flag *******************************************
Description : Clear an interrupt's flag for the virtual machine.
Input       : struct RVM_Virt* Virt - The virtual machine to set flag for.
              ptr_t Int_Num - The interrupt number to set flag for.
Output      : None.
Return      : None.
******************************************************************************/
void _RVM_Clr_Int_Flag(struct RVM_Virt* Virt, ptr_t Int_Num)
{    
    Virt->Image->Int_Flags->Flags[Int_Num>>RVM_WORD_ORDER]&=~(((ptr_t)1)<<(Int_Num&RVM_WORD_MASK));
}
/* End Function:_RVM_Clr_Int_Flag ********************************************/

/* Begin Function:RVM_Hyp_Enable_Int ******************************************
Description : Enable the interrupt for a virtual machine. Need to call this when
              the virtual machine fave finished all its initialization routines
              or it wouldn't be able to receive interrupts.
Input       : None.
Output      : None.
Return      : ret_t - If successful, 0; else an error code.
******************************************************************************/
ret_t RVM_Hyp_Enable_Int(void)
{
    if((RVM_Cur_Virt->Sched.State&RVM_VM_INTENA)!=0)
        return RVM_ERR_STATE;
    else
    {
        /* If the virtual machine ever calls this, then it means that it have done booting */
        if((RVM_Cur_Virt->Sched.State&RVM_VM_BOOTDONE)==0)
        {
            RVM_Cur_Virt->Sched.State|=RVM_VM_BOOTDONE;
            RVM_ASSERT(RVM_Thd_Sched_Prio(RVM_Cur_Virt->Cap.Int_Thd, RVM_VINT_PRIO)==0);
            /* Because some toolchain will reinitialize the memory to zero, we need to init again */
            RVM_Thd_Stack_Init((ptr_t)(RVM_Cur_Virt->Image->Int_Stack),RVM_Cur_Virt->Image->Int_Size,0,0,0,0);
        }
        RVM_Cur_Virt->Sched.State|=RVM_VM_INTENA;
        /* See if we do have excess interrupt to process. If yes, send to the endpoint now */
        if(_RVM_Check_Int_Pend(RVM_Cur_Virt)!=0)
            RVM_Sig_Snd(RVM_Cur_Virt->Cap.Int_Sig);
    }

    return 0;
}
/* End Function:RVM_Hyp_Enable_Int *******************************************/

/* Begin Function:RVM_Hyp_Disable_Int *****************************************
Description : Disable the interrupt for the virtual machine. All interrupts for a
              virtual machine, including the tick timer interrupt,is disabled on
              startup.
Input       : None.
Output      : None.
Return      : ret_t - If successful, 0; else an error code.
******************************************************************************/
ret_t RVM_Hyp_Disable_Int(void)
{
    if((RVM_Cur_Virt->Sched.State&RVM_VM_INTENA)==0)
        return RVM_ERR_STATE;
    else
        RVM_Cur_Virt->Sched.State&=~RVM_VM_INTENA;

    return 0;
}
/* End Function:RVM_Hyp_Disable_Int ******************************************/

/* Begin Function:RVM_Hyp_Reg_Evt *********************************************
Description : Register an event channel. Any sends to this channel will lead to
              one interrupt bit set, and an interrupt sent to this virtual machine.
Input       : ptr_t Int_Num - The interrupt number to send to once there is an event.
              ptr_t VMID - The ID of the VM that is allowed to send to it.
              ptr_t Stack - The stack of the interrupt.
Output      : None.
Return      : ret_t - If successful, the event channel ID; else an error code.
******************************************************************************/
ret_t RVM_Hyp_Reg_Evt(ptr_t Int_Num, ptr_t VMID)
{
    struct RVM_Evt* Event;
    
    /* See if both numbers are overrange */
    if((VMID>=RVM_VM_Num)||(Int_Num>=RVM_MAX_INTVECT))
        return RVM_ERR_RANGE;
    
    /* See if there are empty events available */
    if(RVM_Evt_Free.Next==&RVM_Evt_Free)
        return RVM_ERR_EVENT;
    
    /* Insert this event into the current VM's event list */
    Event=(struct RVM_Evt*)(RVM_Evt_Free.Next);
    RVM_List_Del(Event->Head.Prev,Event->Head.Next);
    RVM_List_Ins(&(Event->Head),RVM_Virt_DB[VMID].Evt_Head.Prev, &(RVM_Virt_DB[VMID].Evt_Head));
    
    Event->State=RVM_EVT_USED;
    Event->Int_Num=Int_Num;
    Event->Rcv_VM_ID=RVM_VMID(RVM_Cur_Virt);
    Event->Snd_VM_ID=VMID;
    
    return 0;
}
/* End Function:RVM_Hyp_Reg_Evt **********************************************/

/* Begin Function:RVM_Hyp_Del_Evt *********************************************
Description : Delete the event channel from the database. Only the creator of the
              event can delete the event.
Input       : None.
Output      : None.
Return      : ret_t - If successful, 0; else an error code.
******************************************************************************/
ret_t RVM_Hyp_Del_Evt(cnt_t Evt_ID)
{
    /* See if the number is overrange */
    if(Evt_ID>=RVM_MAX_EVT_NUM)
        return RVM_ERR_RANGE;
    
    /* Is the block empty? */
    if(RVM_Evt_DB[Evt_ID].State==RVM_EVT_FREE)
        return RVM_ERR_STATE;
    
    /* Are we the creator of the event? */
    if(RVM_Evt_DB[Evt_ID].Rcv_VM_ID!=RVM_VMID(RVM_Cur_Virt))
        return RVM_ERR_EVENT;
    
    /* Delete it from database */
    RVM_Evt_DB[Evt_ID].State=RVM_EVT_FREE;
    RVM_List_Del(RVM_Evt_DB[Evt_ID].Head.Prev,RVM_Evt_DB[Evt_ID].Head.Next);
    RVM_List_Ins(&(RVM_Evt_DB[Evt_ID].Head),RVM_Evt_Free.Prev, &(RVM_Evt_Free));
    
    return 0;
}
/* End Function:RVM_Hyp_Del_Evt **********************************************/

/* Begin Function:RVM_Hyp_Wait_Evt ********************************************
Description : Set the virtual machine to sleep until a vector or an event comes in.
Input       : None.
Output      : None.
Return      : ret_t - If successful, 0; else an error code.
******************************************************************************/
ret_t RVM_Hyp_Wait_Evt(void)
{
    struct RVM_Virt* Next;
    
    /* See if it have interrupt disabled */
    if((RVM_Cur_Virt->Sched.State&RVM_VM_INTENA)==0)
        return RVM_ERR_STATE;

    /* Now suspend the virtual machine and switch to somebody else */
    RVM_VM_STATE_SET(RVM_Cur_Virt->Sched.State,RVM_VM_WAITEVT);
    _RVM_Clr_Rdy(RVM_Cur_Virt);
    /* Insert this into the suspended queue */
    RVM_List_Ins(&(RVM_Cur_Virt->Head),RVM_Wait.Prev,&RVM_Wait);
    
    /* The next could be zero, which means that there are no VM running now */
    Next=_RVM_Get_High_Rdy();
    _RVM_Virt_Switch(RVM_Cur_Virt, Next);
    RVM_Cur_Virt=Next;
    
    return 0;
}
/* End Function:RVM_Hyp_Wait_Evt *********************************************/

/* Begin Function:RVM_Hyp_Send_Evt ********************************************
Description : Send an event to the event channel.
Input       : cid_t Evt_ID - The event channel ID.
Output      : None.
Return      : ret_t - If successful, 0; else an error code.
******************************************************************************/
ret_t RVM_Hyp_Send_Evt(ptr_t Evt_ID)
{
    struct RVM_Virt* Rcv_Virt;
    
    /* See if the number is overrange */
    if(Evt_ID>=RVM_MAX_EVT_NUM)
        return RVM_ERR_RANGE;
    
    /* Is the block empty? */
    if(RVM_Evt_DB[Evt_ID].State==RVM_EVT_FREE)
        return RVM_ERR_STATE;
    
    /* Are we allowed to send to it? */
    if(RVM_Evt_DB[Evt_ID].Snd_VM_ID!=RVM_VMID(RVM_Cur_Virt))
        return RVM_ERR_EVENT;
    
    /* Sending to that VM */
    Rcv_Virt=&RVM_Virt_DB[RVM_Evt_DB[Evt_ID].Rcv_VM_ID];
    _RVM_Set_Int_Flag(Rcv_Virt, RVM_Evt_DB[Evt_ID].Int_Num);
    RVM_ASSERT(RVM_Sig_Snd(Rcv_Virt->Cap.Int_Sig));
    
    if(RVM_VM_STATE(Rcv_Virt->Sched.State)==RVM_VM_WAITEVT)
    {
        RVM_VM_STATE_SET(RVM_Cur_Virt->Sched.State,RVM_VM_RUNNING);
        RVM_List_Del(Rcv_Virt->Head.Prev,Rcv_Virt->Head.Next);
        _RVM_Set_Rdy(Rcv_Virt);
        /* If the receiving VM have a higher priority, switch to it */
        if(Rcv_Virt->Image->Prio>RVM_Cur_Virt->Image->Prio)
        {
            _RVM_Virt_Switch(RVM_Cur_Virt, Rcv_Virt);
            RVM_Cur_Virt=Rcv_Virt;
        }
    }
    
    return 0;
}
/* End Function:RVM_Hyp_Send_Evt *********************************************/

/* Begin Function:RVM_Hyp_Query_Evt *******************************************
Description : Search for a event channel that this VM can send to. Only the first
              channel that matches the criterion will be returned. This function 
              is nondeterminstic and not real-time. Only call this at system startup 
              time is best.
Input       : ptr_t VMID - The VM identifier.
Output      : None.
Return      : ret_t - If successful, the event channel ID; else an error code.
******************************************************************************/
ret_t RVM_Hyp_Query_Evt(ptr_t VMID)
{
    struct RVM_Evt* Evt;
    /* See if both numbers are overrange */
    if(VMID>=RVM_VM_Num)
        return RVM_ERR_RANGE;
    
    Evt=(struct RVM_Evt*)(RVM_Virt_DB[VMID].Evt_Head.Next);
    
    /* Pass through the database to look for the first guy that matches the sender */
    while(Evt!=(struct RVM_Evt*)&(RVM_Virt_DB[VMID].Evt_Head))
    {
        if(Evt->Snd_VM_ID==RVM_VMID(RVM_Cur_Virt))
            return RVM_EVTID(Evt);
        
        Evt=(struct RVM_Evt*)(Evt->Head.Next);
    }
    
    return RVM_ERR_EVENT;
}
/* End Function:RVM_Hyp_Query_Evt ********************************************/

/* Begin Function:RVM_Hyp_Query ***********************************************
Description : This will query the VM ID from its name.
Input       : s8* Name - The name of the VM, 16 characters max, we only compare the first 15.
Output      : None.
Return      : ret_t - If successful, the VM ID; else an error code.
******************************************************************************/
ret_t RVM_Hyp_Query(s8* Name)
{
    cnt_t Count;
    
    /* Pass through the database to look for the first guy that matches the name */
    for(Count=0;Count<RVM_VM_Num;Count++)
    {
        if(_RVM_Strncmp(RVM_Virt_DB[Count].Image->Name,Name,16)==0)
            return Count;
    }
    
    return RVM_ERR_RANGE;
}
/* End Function:RVM_Hyp_Query ************************************************/

/* Begin Function:RVM_Hyp_Tim_Prog ********************************************
Description : Reprgram the timer of this VM, so that the next interrupts will come at
              this point.
Input       : ptr_t Period - The new timer period.
Output      : None.
Return      : ret_t - If successful, 0; else an error code.
******************************************************************************/
ret_t RVM_Hyp_Tim_Prog(ptr_t Period)
{
    /* See if the period is too much */
    if((Period<RVM_MIN_PERIOD)||(Period>=RVM_MAX_PERIOD))
        return RVM_ERR_RANGE;
    
    /* Set this as the new period */
    RVM_Cur_Virt->Sched.Period=Period;
    
    /* Need working timer subsystem */
    RVM_List_Del(RVM_Cur_Virt->Evt_Head.Prev,RVM_Cur_Virt->Evt_Head.Next);
    _RVM_Wheel_Ins(RVM_Cur_Virt,Period);
    
    return 0;
}
/* End Function:RVM_Hyp_Tim_Prog *********************************************/

/* Begin Function:RVM_Hyp_Print ***********************************************
Description : Print a debug string. The print buffer position is predetermined.
Input       : None.
Output      : None.
Return      : ret_t - If successful, 0; else an error code.
******************************************************************************/
ret_t RVM_Hyp_Print(void)
{
    if(RVM_Cur_Virt->Image->Console_Size!=0)
    {
        /* Place the end here to avoid excessive printing */
        ((s8*)RVM_Cur_Virt->Image->Console_Buf)[RVM_Cur_Virt->Image->Console_Size-1]='\0';
        RVM_Print_String(RVM_Cur_Virt->Image->Name);
        RVM_Print_String((s8*)" : ");
        RVM_Print_String(RVM_Cur_Virt->Image->Console_Buf);
        RVM_Print_String((s8*)"\r\n");
    }
    else
        return RVM_ERR_RANGE;
    
    return 0;
}
/* End Function:RVM_Hyp_Print ***********************************************/

/* Print a string into the array */

/* End Of File ***************************************************************/

/* Copyright (C) Evo-Devo Instrum. All rights reserved ***********************/
