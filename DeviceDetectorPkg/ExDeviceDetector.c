/**
  Simple UEFI Memory Test Application - 逐行解釋版
  最簡單的 UEFI 記憶體測試工具實作
**/

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
// 記憶體分配函數：提供 AllocatePool(), FreePool() 等

// ========== 常數定義區 ==========
// 測試 10MB 記憶體
#define TEST_MEMORY_SIZE    (10 * 1024 * 1024)
// 定義要測試的總記憶體大小：10 * 1024 * 1024 = 10,485,760 bytes = 10MB

// 測試區塊大小 1MB
#define TEST_BLOCK_SIZE     (1024 * 1024)
// 每次測試的區塊大小：1024 * 1024 = 1,048,576 bytes = 1MB

// 測試圖樣
#define TEST_PATTERN        0x55AA55AA
// 預設測試圖樣：二進位 01010101 10101010 01010101 10101010
// 用於檢測相鄰位元干擾

/**
  簡單的記憶體測試函數
  
  @param[in] Buffer   要測試的記憶體緩衝區
  @param[in] Size     緩衝區大小（bytes）
  @param[in] Pattern  測試圖樣
  
  @retval TRUE  測試通過
  @retval FALSE 測試失敗
**/
BOOLEAN
SimpleMemoryTest (
  IN VOID   *Buffer,    // IN 表示輸入參數，VOID* 是通用指標
  IN UINTN  Size,       // UINTN 是 UEFI 的無號整數型別（32/64位元視平台而定）
  IN UINT32 Pattern     // UINT32 是 32 位元無號整數
  )
{
  UINT32  *TestBuffer;  // 宣告一個 32 位元整數指標
  UINTN   Index;        // 迴圈計數器
  UINTN   DwordCount;   // DWORD (32-bit) 的數量
  BOOLEAN TestPass = TRUE;  // 測試結果旗標，預設為通過
  
  // 將 Buffer 轉換為 32-bit 指標
  TestBuffer = (UINT32 *)Buffer;
  // 將通用指標 (VOID*) 轉型為 UINT32*，這樣可以以 32-bit 為單位操作
  
  DwordCount = Size / sizeof(UINT32);
  // 計算有多少個 32-bit 整數
  // 例如：1MB = 1,048,576 bytes ÷ 4 bytes = 262,144 個 DWORD
  
  // ========== 步驟 1: 寫入測試圖樣 ==========
  Print(L"  Writing pattern 0x%08x...", Pattern);
  // Print() 是 UEFI 的列印函數，L"" 表示寬字元字串 (Unicode)
  // %08x 表示以 8 位數的 16 進位顯示，不足補 0
  
  for (Index = 0; Index < DwordCount; Index++) {
    TestBuffer[Index] = Pattern;
    // 將測試圖樣寫入每一個 32-bit 位置
    // TestBuffer[0] = Pattern, TestBuffer[1] = Pattern, ...
  }
  
  Print(L" Done\n");
  // 寫入完成，列印 "Done" 並換行
  
  // ========== 步驟 2: 讀取並驗證 ==========
  Print(L"  Verifying pattern...");
  
  for (Index = 0; Index < DwordCount; Index++) {
    if (TestBuffer[Index] != Pattern) {
      // 讀取記憶體內容並與原始圖樣比較
      // 如果不相符，表示記憶體有問題
      
      Print(L"\n  Error at offset 0x%x: Expected 0x%08x, Got 0x%08x\n",
            Index * sizeof(UINT32),  // 錯誤位置的位元組偏移量
            Pattern,                  // 預期的值
            TestBuffer[Index]);       // 實際讀到的值
      
      TestPass = FALSE;  // 標記測試失敗
      break;  // 發現錯誤就停止，不再繼續測試
    }
  }
  
  if (TestPass) {
    Print(L" Pass\n");  // 測試通過
  } else {
    Print(L" Fail\n");  // 測試失敗
  }
  
  return TestPass;  // 回傳測試結果：TRUE = 通過，FALSE = 失敗
}

/**
  UEFI Application Entry Point
  應用程式進入點 - 相當於 C 語言的 main()
**/
EFI_STATUS        // 回傳值型別：EFI_STATUS 是 UEFI 的標準錯誤碼
EFIAPI            // 呼叫慣例：確保參數傳遞方式符合 UEFI 規範
UefiMain (        // 函數名稱：UEFI 應用程式的進入點必須叫 UefiMain
  IN EFI_HANDLE        ImageHandle,  // 這個應用程式的 handle (識別碼)
  IN EFI_SYSTEM_TABLE  *SystemTable  // 系統表指標，包含所有系統服務
  )
{
  EFI_STATUS    Status;         // 儲存函數回傳狀態
  VOID          *TestBuffer;    // 測試用的記憶體緩衝區指標
  UINTN         TestedSize;     // 已測試的記憶體大小
  UINTN         BlockNumber;    // 目前測試的區塊編號
  BOOLEAN       AllTestsPass = TRUE;  // 所有測試是否都通過
  EFI_INPUT_KEY Key;            // 儲存鍵盤輸入
  
  // ========== 清除螢幕並顯示標題 ==========
  gST->ConOut->ClearScreen(gST->ConOut);
  // gST 是全域 System Table 指標
  // ConOut 是 Console Output Protocol
  // ClearScreen() 清除整個螢幕
  
  Print(L"===========================================\n");
  Print(L"    Simple UEFI Memory Test Tool v1.0\n");
  Print(L"===========================================\n\n");
  // 顯示程式標題
  
  // ========== 顯示測試參數 ==========
  Print(L"Test Parameters:\n");
  Print(L"  Total Memory to Test: %d MB\n", TEST_MEMORY_SIZE / (1024 * 1024));
  // 將 bytes 轉換為 MB：10,485,760 ÷ 1,048,576 = 10 MB
  
  Print(L"  Block Size: %d KB\n", TEST_BLOCK_SIZE / 1024);
  // 將 bytes 轉換為 KB：1,048,576 ÷ 1024 = 1024 KB
  
  Print(L"  Test Pattern: 0x%08x\n\n", TEST_PATTERN);
  // 顯示測試圖樣：0x55AA55AA

  // ========== 步驟 1: 分配測試記憶體 ==========
  Print(L"Allocating test memory...\n");
  
  TestBuffer = AllocatePool(TEST_BLOCK_SIZE);
  // AllocatePool() 從 UEFI 記憶體池分配記憶體
  // 分配 TEST_BLOCK_SIZE (1MB) 的記憶體
  // 回傳記憶體起始位址，失敗回傳 NULL
  
  if (TestBuffer == NULL) {
    // 檢查記憶體分配是否成功
    Print(L"ERROR: Failed to allocate memory!\n");
    Print(L"System may not have enough free memory.\n");
    Status = EFI_OUT_OF_RESOURCES;  // 設定錯誤碼：資源不足
    goto Exit;  // 跳到 Exit 標籤處理結束流程
  }
  
  Print(L"Memory allocated successfully at 0x%p\n\n", TestBuffer);
  // %p 是指標格式，顯示記憶體位址
  
  // ========== 步驟 2: 執行記憶體測試 ==========
  Print(L"Starting memory test...\n");
  Print(L"----------------------------------------\n");
  
  TestedSize = 0;   // 初始化已測試大小為 0
  BlockNumber = 0;  // 初始化區塊編號為 0
  
  // 測試多個區塊，直到達到 TEST_MEMORY_SIZE
  while (TestedSize < TEST_MEMORY_SIZE) {
    // 當已測試大小 < 10MB 時繼續測試
    
    Print(L"Testing Block %d:\n", BlockNumber);
    // 顯示目前測試的區塊編號
    
    // ========== 測試 1: 全 0 測試 ==========
    Print(L"Test 1 - All Zeros (0x00000000):\n");
    if (!SimpleMemoryTest(TestBuffer, TEST_BLOCK_SIZE, 0x00000000)) {
      // 呼叫測試函數，傳入：緩衝區、大小、全0圖樣
      // ! 表示 NOT，如果回傳 FALSE (測試失敗)
      AllTestsPass = FALSE;  // 標記整體測試失敗
    }
    
    // ========== 測試 2: 全 1 測試 ==========
    Print(L"Test 2 - All Ones (0xFFFFFFFF):\n");
    if (!SimpleMemoryTest(TestBuffer, TEST_BLOCK_SIZE, 0xFFFFFFFF)) {
      // 測試全1圖樣 (所有位元都是1)
      AllTestsPass = FALSE;
    }
    
    // ========== 測試 3: 交替圖樣測試 ==========
    Print(L"Test 3 - Alternating (0x55AA55AA):\n");
    if (!SimpleMemoryTest(TestBuffer, TEST_BLOCK_SIZE, 0x55AA55AA)) {
      // 0x55 = 01010101, 0xAA = 10101010
      // 用於測試相鄰位元之間的干擾
      AllTestsPass = FALSE;
    }
    
    // ========== 測試 4: 反向交替圖樣 ==========
    Print(L"Test 4 - Inverted (0xAA55AA55):\n");
    if (!SimpleMemoryTest(TestBuffer, TEST_BLOCK_SIZE, 0xAA55AA55)) {
      // 0x55AA55AA 的反向，確保每個位元都能切換
      AllTestsPass = FALSE;
    }
    
    TestedSize += TEST_BLOCK_SIZE;  // 更新已測試大小 (+1MB)
    BlockNumber++;                   // 更新區塊編號 (+1)
    
    // 顯示進度
    Print(L"Progress: %d MB / %d MB tested\n", 
          TestedSize / (1024 * 1024),        // 已測試的 MB 數
          TEST_MEMORY_SIZE / (1024 * 1024)); // 總共要測試的 MB 數
    Print(L"----------------------------------------\n");
  }
  
  // ========== 步驟 3: 釋放記憶體 ==========
  FreePool(TestBuffer);
  // 將分配的記憶體歸還給系統
  // 這是良好的程式設計習慣，避免記憶體洩漏
  
  // ========== 步驟 4: 顯示測試結果 ==========
  Print(L"\n===========================================\n");
  Print(L"            TEST RESULTS\n");
  Print(L"===========================================\n");
  
  if (AllTestsPass) {
    // 如果所有測試都通過
    Print(L"Status: PASS - All memory tests completed successfully!\n");
    Print(L"No errors detected.\n");
    Status = EFI_SUCCESS;  // 設定成功狀態碼
  } else {
    // 如果有任何測試失敗
    Print(L"Status: FAIL - Memory errors detected!\n");
    Print(L"WARNING: System memory may be faulty.\n");
    Status = EFI_DEVICE_ERROR;  // 設定裝置錯誤狀態碼
  }
  
  Print(L"Total Memory Tested: %d MB\n", TEST_MEMORY_SIZE / (1024 * 1024));
  // 顯示總共測試了多少記憶體
  
Exit:  // Exit 標籤，用於 goto 跳轉
  // ========== 等待使用者按鍵 ==========
  Print(L"\nPress any key to exit...");
  
  gST->ConIn->Reset(gST->ConIn, FALSE);
  // 重置輸入緩衝區，清除之前的按鍵
  // FALSE 表示不要執行延伸重置
  
  while (gST->ConIn->ReadKeyStroke(gST->ConIn, &Key) == EFI_NOT_READY);
  // 迴圈等待直到讀取到按鍵
  // ReadKeyStroke() 嘗試讀取按鍵
  // 如果沒有按鍵，回傳 EFI_NOT_READY，繼續等待
  // 如果有按鍵，回傳 EFI_SUCCESS，結束迴圈
  
  return Status;
  // 回傳最終狀態給 UEFI
  // EFI_SUCCESS = 成功
  // EFI_DEVICE_ERROR = 裝置錯誤
  // EFI_OUT_OF_RESOURCES = 資源不足
}