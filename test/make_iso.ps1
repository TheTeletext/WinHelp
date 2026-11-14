# make_iso.ps1 - put files on an ISO 9660 CD image with Windows' own IMAPI2
# (no third-party tool), for the WfW 3.11 VM's CD drive: the guest reads the
# files from the CD, so nothing new is written to the VM disk (phase 1 s7).
param([Parameter(Mandatory=$true)][string]$Iso, [Parameter(Mandatory=$true)][string[]]$Files,
      [string]$Label = "TTHELP")
$img = New-Object -ComObject IMAPI2FS.MsftFileSystemImage
$img.FileSystemsToCreate = 1          # FsiFileSystemISO9660: 8.3 names, as DOS reads them
$img.VolumeName = $Label
$img.ChooseImageDefaultsForMediaType(12)   # IMAPI_MEDIA_TYPE_DISK (CD/DVD-size image)
foreach ($f in $Files) { $img.Root.AddTree((Resolve-Path $f).Path, $false) }
$res = $img.CreateResultImage()
$stream = $res.ImageStream
# copy the IStream to a file
if (-not ("IsoCopy" -as [type])) {
Add-Type -TypeDefinition @'
using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.ComTypes;
public class IsoCopy {
  public static void Save(object o, string path, int blocks, int blockSize) {
    IStream s = (IStream)o;
    using (var fs = File.Create(path)) {
      byte[] buf = new byte[blockSize];
      IntPtr read = Marshal.AllocHGlobal(4);
      for (int i = 0; i < blocks; i++) {
        s.Read(buf, blockSize, read);
        int n = Marshal.ReadInt32(read);
        fs.Write(buf, 0, n);
      }
      Marshal.FreeHGlobal(read);
    }
  }
}
'@
}
[IsoCopy]::Save($stream, $Iso, $res.TotalBlocks, $res.BlockSize)
"{0}: {1:N0} B, {2}" -f $Iso, (Get-Item $Iso).Length, (($Files | ForEach-Object { Split-Path $_ -Leaf }) -join ", ")
