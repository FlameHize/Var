#ifndef VAR_PCIE_PCIEGLOABEL_H
#define VAR_PCIE_PCIEGLOABEL_H

#define MAP_SIZE                (1024*1024UL)
//#define PACKAGE_SIZE            (4*1024*1024)
//#define PACKAGE_SIZE            (64*1024*1024)
#define DATA_SIZE               (64*1024*1024)
#define PACKAGE_SIZE            (300*1024*1024)
#define LOOP_TIMES              (16)
#define ALIGN_SIZE              (4096)//2048//2048//512//1024//4096 //096   //2048

#define XDMACHANNELNUM          (8)//(5)
#define DDRSPLITSIZE            (6)
#define SINGLEDDRSPLITSIZE      (4)
#define MAP_SIZE                (1024*1024UL)
#define ONCEBYTES               (64)//4096  //largest=4096
#define WRITE_DATA_PTR  40//84 //10

/**************************************************************************************
  PCIE 下行指令
  *************************************************************************************/
#define WRITE_RESET_REG               (4*3)  //reset reg3.bit0 dma reset,bit1 aurora reset
#define WRITE_C2H_DDR_START_1         (4*5)  //pcie C2H channel DDR_1 head addr
#define WRITE_C2H_DDR_SIZE_1          (4*6)  //pcie C2H channel DDR_1 size
#define WRITE_H2C_DDR_START           (4*7)  //pcie H2C channel DDR head addr
#define WRITE_H2C_DDR_SIZE            (4*8)  //pcie H2C channel DDR size
#define WRITE_H2C_DDR_PTR             (4*9)  //pcie H2C channel DDR pointer
#define WRITE_C2H_DDR_START_2         (4*10)  //pcie C2H channel DDR_2 head addr
#define WRITE_C2H_DDR_SIZE_2          (4*11)  //pcie C2H channel DDR_2 size
#define WRITE_C2H_DDR_START_3         (4*12)  //pcie C2H channel DDR_3 head addr
#define WRITE_C2H_DDR_SIZE_3          (4*13)  //pcie C2H channel DDR_3 size
#define WRITE_C2H_DDR_START_4         (4*14)  //pcie C2H channel DDR_4 head addr
#define WRITE_C2H_DDR_SIZE_4          (4*15)  //pcie C2H channel DDR_4 size
#define WRITE_C2H_DDR_START_5         (4*16)  //pcie C2H channel DDR_5 head addr
#define WRITE_C2H_DDR_SIZE_5          (4*17)  //pcie C2H channel DDR_5 size
#define WRITE_C2H_DDR_START_6         (4*18)  //pcie C2H channel DDR_6 head addr
#define WRITE_C2H_DDR_SIZE_6          (4*19)  //pcie C2H channel DDR_6 size

#define WRITE_C2H_DDR_TYPE            (4*20)  //pcie C2H channel DDR_6 size
#define WRITE_C2H_DDR_LENGTH          (4*21)  //pcie C2H channel DDR_6 size
#define WRITE_C2H_DDR_TAIL            (4*22)  //pcie C2H channel DDR_6 size



/**************************************************************************************
  PCIE 接收指令
  *************************************************************************************/
#define READ_SOFTWARE_ID              (4*0)  //reset reg3.bit0 dma reset,bit1 aurora reset
#define READ_C2H_DDR0_INDEX           (4*1)  //pcie C2H channel DDR_1 head addr
#define READ_C2H_DDR0_TYPE            (4*2)  //pcie C2H channel DDR_1 size
#define READ_H2C_DDR0_START_FILE1     (4*3)  //pcie H2C channel DDR head addr
#define READ_H2C_DDR0_END_FILE1       (4*4)  //pcie H2C channel DDR size
#define READ_H2C_DDR0_START_FILE2     (4*5)  //pcie H2C channel DDR head addr
#define READ_H2C_DDR0_END_FILE2       (4*6)  //pcie H2C channel DDR size
#define READ_H2C_DDR0_START_FILE3     (4*7)  //pcie H2C channel DDR head addr
#define READ_H2C_DDR0_END_FILE3       (4*8)  //pcie H2C channel DDR size
#define READ_H2C_DDR0_START_FILE4     (4*9)  //pcie H2C channel DDR head addr
#define READ_H2C_DDR0_END_FILE4       (4*10)  //pcie H2C channel DDR size

#define READ_C2H_DDR1_INDEX           (4*11)  //pcie C2H channel DDR_1 head addr
#define READ_C2H_DDR1_TYPE            (4*12)  //pcie C2H channel DDR_1 size
#define READ_H2C_DDR1_START_FILE1     (4*13)  //pcie H2C channel DDR head addr
#define READ_H2C_DDR1_END_FILE1       (4*14)  //pcie H2C channel DDR size
#define READ_H2C_DDR1_START_FILE2     (4*15)  //pcie H2C channel DDR head addr
#define READ_H2C_DDR1_END_FILE2       (4*16)  //pcie H2C channel DDR size
#define READ_H2C_DDR1_START_FILE3     (4*17)  //pcie H2C channel DDR head addr
#define READ_H2C_DDR1_END_FILE3       (4*18)  //pcie H2C channel DDR size
#define READ_H2C_DDR1_START_FILE4     (4*19)  //pcie H2C channel DDR head addr
#define READ_H2C_DDR1_END_FILE4       (4*20)  //pcie H2C channel DDR size

#define READ_C2H_DDR2_INDEX           (4*21)  //pcie C2H channel DDR_1 head addr
#define READ_C2H_DDR2_TYPE            (4*22)  //pcie C2H channel DDR_1 size
#define READ_H2C_DDR2_START_FILE1     (4*23)  //pcie H2C channel DDR head addr
#define READ_H2C_DDR2_END_FILE1       (4*24)  //pcie H2C channel DDR size
#define READ_H2C_DDR2_START_FILE2     (4*25)  //pcie H2C channel DDR head addr
#define READ_H2C_DDR2_END_FILE2       (4*26)  //pcie H2C channel DDR size
#define READ_H2C_DDR2_START_FILE3     (4*27)  //pcie H2C channel DDR head addr
#define READ_H2C_DDR2_END_FILE3       (4*28)  //pcie H2C channel DDR size
#define READ_H2C_DDR2_START_FILE4     (4*29)  //pcie H2C channel DDR head addr
#define READ_H2C_DDR2_END_FILE4       (4*30)  //pcie H2C channel DDR size

#define READ_C2H_DDR3_INDEX           (4*31)  //pcie C2H channel DDR_1 head addr
#define READ_C2H_DDR3_TYPE            (4*32)  //pcie C2H channel DDR_1 size
#define READ_H2C_DDR3_START_FILE1     (4*33)  //pcie H2C channel DDR head addr
#define READ_H2C_DDR3_END_FILE1       (4*34)  //pcie H2C channel DDR size
#define READ_H2C_DDR3_START_FILE2     (4*35)  //pcie H2C channel DDR head addr
#define READ_H2C_DDR3_END_FILE2       (4*36)  //pcie H2C channel DDR size
#define READ_H2C_DDR3_START_FILE3     (4*37)  //pcie H2C channel DDR head addr
#define READ_H2C_DDR3_END_FILE3       (4*38)  //pcie H2C channel DDR size
#define READ_H2C_DDR3_START_FILE4     (4*39)  //pcie H2C channel DDR head addr
#define READ_H2C_DDR3_END_FILE4       (4*40)  //pcie H2C channel DDR size

#define READ_C2H_DDR4_INDEX           (4*41)  //pcie C2H channel DDR_1 head addr
#define READ_C2H_DDR4_TYPE            (4*42)  //pcie C2H channel DDR_1 size
#define READ_H2C_DDR4_START_FILE1     (4*43)  //pcie H2C channel DDR head addr
#define READ_H2C_DDR4_END_FILE1       (4*44)  //pcie H2C channel DDR size
#define READ_H2C_DDR4_START_FILE2     (4*45)  //pcie H2C channel DDR head addr
#define READ_H2C_DDR4_END_FILE2       (4*46)  //pcie H2C channel DDR size
#define READ_H2C_DDR4_START_FILE3     (4*47)  //pcie H2C channel DDR head addr
#define READ_H2C_DDR4_END_FILE3       (4*48)  //pcie H2C channel DDR size
#define READ_H2C_DDR4_START_FILE4     (4*49)  //pcie H2C channel DDR head addr
#define READ_H2C_DDR4_END_FILE4       (4*50)  //pcie H2C channel DDR size

#define READ_C2H_DDR5_INDEX           (4*51)  //pcie C2H channel DDR_1 head addr
#define READ_C2H_DDR5_TYPE            (4*52)  //pcie C2H channel DDR_1 size
#define READ_H2C_DDR5_START_FILE1     (4*53)  //pcie H2C channel DDR head addr
#define READ_H2C_DDR5_END_FILE1       (4*54)  //pcie H2C channel DDR size
#define READ_H2C_DDR5_START_FILE2     (4*55)  //pcie H2C channel DDR head addr
#define READ_H2C_DDR5_END_FILE2       (4*56)  //pcie H2C channel DDR size
#define READ_H2C_DDR5_START_FILE3     (4*57)  //pcie H2C channel DDR head addr
#define READ_H2C_DDR5_END_FILE3       (4*58)  //pcie H2C channel DDR size
#define READ_H2C_DDR5_START_FILE4     (4*59)  //pcie H2C channel DDR head addr
#define READ_H2C_DDR5_END_FILE4       (4*60)  //pcie H2C channel DDR size

//uint g_uiDDRSizeDesign[XDMACHANNELNUM][DDRSPLITSIZE][2];

//设置CPU向下发送数据的缓存区大小
#define CPUSENDBUFFERSIZE  (40000)

#endif // VAR_PCIE_PCIEGLOABEL_H
