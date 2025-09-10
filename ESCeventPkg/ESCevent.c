#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/SimpleFileSystem.h>
#include <Guid/FileInfo.h>
#include <Library/BaseLib.h>
#include <IndustryStandard/Bmp.h>
#include <Library/MemoryAllocationLib.h>

EFI_STATUS ConvertBmpToBlt(VOID *BmpImage, UINTN BmpImageSize, VOID **GopBlt, UINTN *GopBltSize, UINTN *PixelHeight, UINTN *PixelWidth)
{

	UINT8                         *ImageData;
	UINT8                         *ImageBegin;
	BMP_IMAGE_HEADER              *BmpHeader;
	EFI_GRAPHICS_OUTPUT_BLT_PIXEL *BltBuffer;
	EFI_GRAPHICS_OUTPUT_BLT_PIXEL *Blt;
	UINT64                        BltBufferSize;
	UINTN                         Height;
	UINTN                         Width;
	UINTN                         ImageIndex;

	BmpHeader = (BMP_IMAGE_HEADER *)BmpImage;
	ImageBegin = ((UINT8 *)BmpImage) + BmpHeader->ImageOffset;
    
    /* Bmp僅支援24位元或32位元，非者即非Bmp */
	if (BmpHeader->BitPerPixel != 24 && BmpHeader->BitPerPixel != 32)
		return EFI_UNSUPPORTED;

	BltBufferSize = BmpHeader->PixelWidth * BmpHeader->PixelHeight * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL);
	*GopBltSize = (UINTN)BltBufferSize;
	*GopBlt = AllocatePool(*GopBltSize);   
	
	*PixelWidth = BmpHeader->PixelWidth;
	*PixelHeight = BmpHeader->PixelHeight;

	ImageData = ImageBegin;
	BltBuffer = *GopBlt;
	for (Height = 0; Height < BmpHeader->PixelHeight; Height++)
	{
        // Buffer大小=長x寬
		Blt = &BltBuffer[(BmpHeader->PixelHeight-Height-1) * BmpHeader->PixelWidth];
		for (Width = 0; Width < BmpHeader->PixelWidth; Width++, Blt++)
		{
            Blt->Blue = *ImageData++;
			Blt->Green = *ImageData++;
			Blt->Red = *ImageData++;
			/* 僅會有24位元或32位元，
            因為非者已在前面被return，若32位元則ImageData++。
            */
			switch (BmpHeader->BitPerPixel)
			{
			case 24:
				break;
			case 32:
				ImageData++;
				break;
			default:
				break;
			}
		}

		ImageIndex = (UINTN)(ImageData - ImageBegin);
        /* 除以4後，將商+餘數 */
		if ((ImageIndex % 4) != 0)
			ImageData = ImageData + (4 - (ImageIndex % 4));
	}
	
	return EFI_SUCCESS;
}

EFI_STATUS ReadFileToBuffer(
    IN CHAR16 *FileName,
    OUT VOID **FileBuffer,
    OUT UINTN *FileSize)
{
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *SimpleFileSystem;
    EFI_FILE_PROTOCOL *Root;
    EFI_FILE_PROTOCOL *FileHandle;
    EFI_FILE_INFO *FileInfo;
    UINTN InfoSize = 0;

    Status = gBS->LocateProtocol(
        &gEfiSimpleFileSystemProtocolGuid,
        NULL,
        (VOID **)&SimpleFileSystem);
    if (EFI_ERROR(Status))
    {
        Print(L"ERROR: Cannot Locate SimpleFileSystem");
        return Status;
    }
    Status = SimpleFileSystem->OpenVolume(SimpleFileSystem, &Root);
    if (EFI_ERROR(Status))
    {
        Print(L"ERROR: Cannot Open Volume");
        return Status;
    }
    Status = Root->Open(Root, &FileHandle, FileName, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status))
    {
        Print(L"ERROR: Cannot Open File %s", FileName);
        return Status;
    }
    Status = FileHandle->GetInfo(FileHandle, &gEfiFileInfoGuid, &InfoSize, NULL);
    if (Status != EFI_BUFFER_TOO_SMALL)
    {
        // 第一次 GetInfo 失敗但不是因為緩衝區太小，代表有其他問題
        Print(L"ERROR: Cannot First Get info error");
        FileHandle->Close(FileHandle);
        return Status;
    }
    Status = gBS->AllocatePool(EfiBootServicesData, InfoSize, (VOID **)&FileInfo);
    Status = FileHandle->GetInfo(FileHandle, &gEfiFileInfoGuid, &InfoSize, FileInfo);
    if (EFI_ERROR(Status))
    {
        gBS->FreePool(FileInfo);
        FileHandle->Close(FileHandle);
        return Status;
    }
    *FileSize = FileInfo->FileSize;
    gBS->FreePool(FileInfo); // 取得大小後即可釋放
    Status = gBS->AllocatePool(EfiBootServicesData, *FileSize, FileBuffer);
    if (EFI_ERROR(Status))
    {
        Print(L"ERROR: Cannot allocate pool for file.\n");
        FileHandle->Close(FileHandle);
        return Status;
    }
    Status = FileHandle->Read(FileHandle, FileSize, *FileBuffer);
    if (EFI_ERROR(Status))
    {
        Print(L"ERROR: Cannot read file.\n");
        gBS->FreePool(*FileBuffer);
        FileHandle->Close(FileHandle);
        return Status;
    }
    FileHandle->Close(FileHandle);

    return EFI_SUCCESS;
}
VOID ShowMenu(){
    Print(L"Press f to show Furina\nPress ESC to Enter Kernel\n");
}

EFI_STATUS EFIAPI UefiMain(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
    EFI_STATUS Status;
    VOID *KernelBuffer = NULL;
    UINTN KernelSize = 0;
    EFI_MEMORY_DESCRIPTOR *MemoryMap = NULL;
    UINTN MapSize = 0;
    UINTN MapKey = 0;
    UINTN DescriptorSize = 0;
    UINT32 DescriptorVersion = 0;
    // UINTN                   KernelAddress = 0;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *GraphicsProtocol = NULL;
    Status = gBS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid, NULL, (VOID **)&GraphicsProtocol);
    // 【重要】在退出開機服務前，先儲存好所有需要傳遞給核心的資訊
    UINTN FrameBufferBase = GraphicsProtocol->Mode->FrameBufferBase;
    UINTN HorizontalResolution = GraphicsProtocol->Mode->Info->HorizontalResolution;
    UINTN VerticalResolution = GraphicsProtocol->Mode->Info->VerticalResolution;
    
    VOID *ImageBuffer = NULL;
    UINTN ImageSize = 0;
    VOID			  *GopBlt = NULL;
	UINTN			  GopBltSize;
	UINTN			  BmpHeight;
	UINTN			  BmpWidth;

    
    Print(L"OinkBL Bootloader starting...\n");
    EFI_INPUT_KEY Key;
    EFI_INPUT_KEY WaitKey;
    while (1)
    {
        ShowMenu();
        gBS->WaitForEvent(1 , &gST->ConIn->WaitForKey , NULL);
        Status = gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);
        switch (Key.UnicodeChar)
        {
        case 'f':
            Print(L"furina\n");
            ReadFileToBuffer(L"Logo.bmp" , &ImageBuffer ,&ImageSize);
            Status = ConvertBmpToBlt(ImageBuffer, ImageSize, &GopBlt, &GopBltSize, &BmpHeight, &BmpWidth);
            if(EFI_ERROR(Status)){
                Print(L"convert error");
            }
            Status = GraphicsProtocol->Blt(GraphicsProtocol,GopBlt , EfiBltBufferToVideo , 0 , 0 , 500 ,400 , BmpWidth , BmpHeight , 0);
            if(!EFI_ERROR(Status)){
                Print(L"this is furina\n");
            }
            gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, NULL);
            gST->ConIn->ReadKeyStroke(gST->ConIn, &WaitKey);
            break;
        }
        if (Key.ScanCode == SCAN_ESC)
        {
            // 1. 載入核心檔案
            Status = ReadFileToBuffer(L"Kernel.bin", &KernelBuffer, &KernelSize);
            if (EFI_ERROR(Status))
            {
                Print(L"ERROR: Could not load Kernel.bin. Halting.\n");
                return Status;
            }
            Print(L"Kernel.bin loaded at 0x%p (%d bytes)\n", KernelBuffer, KernelSize);

            // 2. 取得記憶體映射並退出開機服務
            gBS->GetMemoryMap(&MapSize, MemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion);
            MapSize += DescriptorSize * 2;
            Status = gBS->AllocatePool(EfiLoaderData, MapSize, (VOID **)&MemoryMap);
            Status = gBS->GetMemoryMap(&MapSize, MemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion);
            if (EFI_ERROR(Status))
            {
                Print(L"ERROR: Failed to get memory map.\n");
                return Status;
            }

            // 退出開機服務
            Status = gBS->ExitBootServices(ImageHandle, MapKey);
            if (EFI_ERROR(Status))
            {
                goto Hang;
            }

            //
            // ==================【 關鍵修正 】==================
            //

            // 1. 將核心的位址轉換成一個「接收三個 UINTN 參數」的函式指標
            void (*KernelEntryPoint)(UINTN FrameBufferAddr, UINTN ScreenWidth, UINTN ScreenHeight);
            KernelEntryPoint = (void (*)(UINTN, UINTN, UINTN))(UINTN)KernelBuffer;
            // 2. 像呼叫普通 C 函式一樣，將參數傳遞給核心並跳轉
            KernelEntryPoint(FrameBufferBase, HorizontalResolution, VerticalResolution);
            //
            // ===============================================
            //
        Hang:
            while (1)
                ;

            // 永遠不會執行到
            UNREACHABLE();
            return Status;
        }

        gBS->Stall(10000);
    }
    return Status;
}