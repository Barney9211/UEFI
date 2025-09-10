// ========== 標頭檔引入區 ==========
#include <Uefi.h>
// UEFI 基本定義：包含 EFI_STATUS, EFI_HANDLE, CHAR16 等基本型別定義

#include <Library/UefiLib.h>
// UEFI Library 函數：提供 Print(), CopyMem() 等常用函數

#include <Library/UefiApplicationEntryPoint.h>
// UEFI 應用程式進入點：定義 UefiMain 作為程式進入點

#include <Library/UefiBootServicesTableLib.h>
// Boot Services 表：提供 gBS 全域變數，用於呼叫 Boot Services

#include <Library/BaseMemoryLib.h>
// 記憶體操作函數：提供 SetMem(), CompareMem() 等記憶體操作

#include <Library/MemoryAllocationLib.h>
#include <Library/BaseLib.h>
/**
 * @brief 將 EFI_MEMORY_TYPE 的整數值轉換為可讀的字串。
 *
 * @param Type  記憶體描述符中的 Type 欄位值。
 * @return      表示該記憶體類型的唯讀字串。
 */
const CHAR16* MemoryTypeToString(UINT32 Type) {
    switch (Type) {
        case EfiReservedMemoryType:         return L"Reserved";
        case EfiLoaderCode:                 return L"Loader Code";
        case EfiLoaderData:                 return L"Loader Data";
        case EfiBootServicesCode:           return L"Boot Services Code";
        case EfiBootServicesData:           return L"Boot Services Data";
        case EfiRuntimeServicesCode:        return L"Runtime Services Code";
        case EfiRuntimeServicesData:        return L"Runtime Services Data";
        case EfiConventionalMemory:         return L"Conventional Memory";
        case EfiUnusableMemory:             return L"Unusable Memory";
        case EfiACPIReclaimMemory:          return L"ACPI Reclaim Memory";
        case EfiACPIMemoryNVS:              return L"ACPI Memory NVS";
        case EfiMemoryMappedIO:             return L"Memory Mapped IO";
        case EfiMemoryMappedIOPortSpace:    return L"Memory Mapped IO Port Space";
        case EfiPalCode:                    return L"PAL Code";
        case EfiPersistentMemory:           return L"Persistent Memory";
        case EfiUnacceptedMemoryType:       return L"Unaccepted Memory";
        default:                            return L"Unknown";
    }
}

/**
 * @brief 迭代並印出 EFI 記憶體映射表中的所有分區資訊。
 *
 * @param MemoryMap         指向記憶體映射表緩衝區的指標。
 * @param MemoryMapSize     整個記憶體映射表的大小 (bytes)。
 * @param DescriptorSize    單一記憶體描述符的大小 (bytes)。
 */
VOID PrintMemoryMap(IN EFI_MEMORY_DESCRIPTOR *MemoryMap, 
                    IN UINTN MemoryMapSize, 
                    IN UINTN DescriptorSize) 
{
    EFI_MEMORY_DESCRIPTOR *Desc;
    UINTN i;

    if (MemoryMap == NULL || MemoryMapSize == 0) {
        Print(L"Memory map is empty or invalid.\n");
        return;
    }

    // 列印標頭
    Print(L"==============================================================================\n");
    Print(L"| Type                      | Physical Start     | Num Pages  | Size (KB)    |\n");
    Print(L"==============================================================================\n");

    Desc = MemoryMap;
    for (i = 0; i < MemoryMapSize / DescriptorSize; i++) {
        // 計算分區大小 (KB)
        UINTN sizeKb = (Desc->NumberOfPages * 4096) / 1024;

        // 印出格式化的資訊
        // %-25s: 左對齊，寬度 25 的字串
        // %016lx: 16 位元組長度的十六進位，不足補 0
        // %-10d: 左對齊，寬度 10 的十進位
        if(Desc->Type==EfiConventionalMemory){
            Print(L"| %-25s | 0x%016lx | %-10d | %-12d |\n",
              MemoryTypeToString(Desc->Type),
              Desc->PhysicalStart,
              Desc->NumberOfPages,
              sizeKb);
        }

        // 移動到下一個描述符
        // 需要將指標轉型為 UINT8* (byte pointer) 再做位移，確保移動的單位是 byte
        Desc = (EFI_MEMORY_DESCRIPTOR*)((UINT8*)Desc + DescriptorSize);
    }

    Print(L"==============================================================================\n");
}
BOOLEAN MemoryTest(
    IN EFI_PHYSICAL_ADDRESS AddressStart,
    IN UINTN NumberOfPages,
    IN UINT32 Pattern
)
{

    UINT32* curPtr = (UINT32 *)(UINTN)AddressStart;
    UINTN length = NumberOfPages*4096;
    //寫入 4096是因為allocatePage一次是分配4KB的頁面 除4是因為Pattern是UINT32 如果是UINT16 就除2
    // for(int i = 0; i < length ; i++){
    //     curPtr[i] = Pattern;
    // }
    SetMem32(curPtr, length * 4, Pattern);
    //讀取並驗證
    
    // curPtr[100] = 0x12345678; //錯誤case
    for(int i = 0; i < length ; i++){
        //驗證失敗
        if(curPtr[i] != Pattern)
        {
            Print(L"Test Failed\n");
            return FALSE;
        }
    }
    Print(L"Test complete %lx\n" , AddressStart);
    return TRUE;
}

EFI_STATUS        // 回傳值型別：EFI_STATUS 是 UEFI 的標準錯誤碼
EFIAPI            // 呼叫慣例：確保參數傳遞方式符合 UEFI 規範
UefiMain (        // 函數名稱：UEFI 應用程式的進入點必須叫 UefiMain
  IN EFI_HANDLE        ImageHandle,  // 這個應用程式的 handle (識別碼)
  IN EFI_SYSTEM_TABLE  *SystemTable  // 系統表指標，包含所有系統服務
){
    UINTN MemoryMapSize = 0;
    VOID* MemoryMap;
    UINTN MapKey;
    UINTN DescriptorSize;
    UINT32 DescriptorVersion;
    EFI_STATUS Status;
    //--------------------------------------------------- get the Memory map
    Status = gBS->GetMemoryMap(&MemoryMapSize , NULL , &MapKey , &DescriptorSize , &DescriptorVersion);
    if(Status != EFI_BUFFER_TOO_SMALL){
        Print(L"Error: Unexpected status from first GetMemoryMap call: %r\n", Status);
        return Status;
    }
    MemoryMapSize *= 2;
    MemoryMap = AllocatePool(MemoryMapSize);
    // Print(L"Memory Map size : %d\n" , MemoryMapSize);
    if(MemoryMap == NULL){
        Print(L"allocate pool error");
        return Status;
    }
    Status = gBS->GetMemoryMap(&MemoryMapSize , (EFI_MEMORY_DESCRIPTOR *)MemoryMap , &MapKey , &DescriptorSize , &DescriptorVersion);
    // Print(L"Memory Map size : %d\n" , MemoryMapSize);
    if(Status == EFI_BUFFER_TOO_SMALL){
        Print(L"Erro to get Memory Map222222");
        return Status;
    }
    // PrintMemoryMap(MemoryMap , MemoryMapSize , DescriptorSize);
    //--------------------------------------------------end get the Memory map
    UINTN length_of_memory_map = MemoryMapSize / DescriptorSize;
    EFI_MEMORY_DESCRIPTOR *temp = MemoryMap;
    EFI_PHYSICAL_ADDRESS physicalStart;

    for(int index = 0 ; index <length_of_memory_map; index++){
        if(temp->Type == EfiConventionalMemory){
            // Print(L"Index : %d , begin address %016lx\n",index , temp->PhysicalStart);
            physicalStart = temp->PhysicalStart;
            gBS->AllocatePages(AllocateAddress , EfiBootServicesData , temp->NumberOfPages , &physicalStart);
            MemoryTest(physicalStart , temp->NumberOfPages , 0xFFFFFFFF);
        }
        temp = (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)temp + DescriptorSize);
    }

    return EFI_SUCCESS;
}