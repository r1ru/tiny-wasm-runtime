#include <efi.h>
#include <efilib.h>

EFI_STATUS efi_main (EFI_HANDLE image, EFI_SYSTEM_TABLE *systab) {
    InitializeLib(image, systab);

    // open root dir
    EFI_STATUS status;
    EFI_LOADED_IMAGE *lip = NULL;
    status = uefi_call_wrapper(
        BS->OpenProtocol, 6, 
        image, 
        &LoadedImageProtocol,
        (void **) &lip,
        image, 
        NULL,
        EFI_OPEN_PROTOCOL_GET_PROTOCOL
    );
    if(EFI_ERROR(status))
        return status;

    EFI_FILE_HANDLE root = LibOpenRoot(lip->DeviceHandle);
    if(root == NULL)
        return EFI_ABORTED;

    // open file
    EFI_FILE_HANDLE fhandle;
    status = uefi_call_wrapper(
        root->Open,
        5, root, &fhandle, L"data.txt", EFI_FILE_MODE_READ, 0
    );
    if(EFI_ERROR(status))
        return status;

    // get file size
    EFI_FILE_INFO *info = LibFileInfo(fhandle);
    UINT64 fsize = info->FileSize;
    FreePool(info);

    // allocate pool
    void *pool = AllocatePool(fsize);
    if(pool == NULL)
        return EFI_ABORTED;
    
    // read file
    status = uefi_call_wrapper(
        fhandle->Read, 3, 
        fhandle, 
        &fsize, 
        pool
    );
    if(EFI_ERROR(status))
        return status;
    
    // print data
    DumpHex(4, 0, fsize, pool);
    
    // close handles
    status = uefi_call_wrapper(fhandle->Close,1, fhandle);
    if(EFI_ERROR(status))
        return status;

    status = uefi_call_wrapper(root->Close, 1, root);
    if(EFI_ERROR(status))
        return status;

    // free pool
    FreePool(pool);

    return EFI_SUCCESS;
}