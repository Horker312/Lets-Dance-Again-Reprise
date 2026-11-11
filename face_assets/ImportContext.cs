using System.IO;
using System.Security.Cryptography;
namespace FaceAssets;
// Per-request ownership. No parsed faces or plugin databases are shared between profiles.
public sealed class ImportContext {
 public CancellationToken Cancellation{get;init;}
 public IProgress<string>? Progress{get;init;}
 public bool AllowUnprovenGeneratedIds{get;init;}=true; // legacy synchronous/manual entry point
 public bool AcceptDeclaredTopology{get;init;}
 public bool SkipGeneratedIds{get;init;}
 public void Stage(string name){Cancellation.ThrowIfCancellationRequested();Progress?.Report(name);}
}
public sealed record ResourceOrigin(string Logical,string Selected,string Backing,string? Archive,string Reason,string Sha256);
public static class ImportFiles {
 public static string Digest(string path){using var f=File.OpenRead(path);return Convert.ToHexString(SHA256.HashData(f));}
 public static bool Expected(Exception e)=>e is InvalidDataException or IOException or UnauthorizedAccessException or System.Text.Json.JsonException or InvalidOperationException or FormatException or ArgumentException or NotSupportedException or OverflowException or KeyNotFoundException;
}
