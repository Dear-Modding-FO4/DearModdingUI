using System.Globalization;
using System.Text;
using System.Text.Json;
using Mutagen.Bethesda;
using Mutagen.Bethesda.Fallout4;
using Mutagen.Bethesda.Plugins;
using Mutagen.Bethesda.Plugins.Records;

const string PluginName = "DMUITests.esp";
const uint QuestFormId = 0x800;

if (args.Length != 3)
{
    Console.Error.WriteLine(
        "Usage: FixtureBuilder <Caprica output directory> <fixture data directory> <staging directory>");
    return 2;
}

var compiledScriptDirectory = Path.GetFullPath(args[0]);
var fixtureDataDirectory = Path.GetFullPath(args[1]);
var stagingDirectory = Path.GetFullPath(args[2]);
var scriptOutputDirectory = Path.Combine(stagingDirectory, "Scripts");
Directory.CreateDirectory(scriptOutputDirectory);

var pluginPath = Path.Combine(stagingDirectory, PluginName);
var questPexPath = Path.Combine(scriptOutputDirectory, "DMUITestQuest.pex");
var functionsPexPath = Path.Combine(scriptOutputDirectory, "DMUITestFunctions.pex");

WritePlugin(pluginPath);
NormalizeCompiledPex(
    Path.Combine(compiledScriptDirectory, "DMUITestQuest.pex"),
    questPexPath,
    "DMUITestQuest.psc");
NormalizeCompiledPex(
    Path.Combine(compiledScriptDirectory, "DMUITestFunctions.pex"),
    functionsPexPath,
    "DMUITestFunctions.psc");

ValidatePlugin(pluginPath, fixtureDataDirectory);

Console.WriteLine($"Validated staged plugin {pluginPath}");
Console.WriteLine($"Normalized and staged {questPexPath}");
Console.WriteLine($"Normalized and staged {functionsPexPath}");
Console.WriteLine(
    "ESP contract: ESL, Fallout4.esm master, QUST 0x800 with DMUITestQuest VMAD, typed GLOB records 0x801-0x804.");
Console.WriteLine("Canonical config record and property references match the generated ESP.");
return 0;

static void WritePlugin(string path)
{
    var modKey = ModKey.FromNameAndExtension(PluginName);
    var fallout4Master = ModKey.FromNameAndExtension("Fallout4.esm");
    var mod = new Fallout4Mod(modKey, Fallout4Release.Fallout4)
    {
        IsSmallMaster = true,
    };
    mod.ModHeader.MasterReferences.Add(new MasterReference { Master = fallout4Master });

    var quest = new Quest(new FormKey(modKey, QuestFormId), Fallout4Release.Fallout4)
    {
        EditorID = "DMUITestQuest",
        Data = new QuestData
        {
            Flags = Quest.Flag.StartGameEnabled | Quest.Flag.RunOnce,
            Type = Quest.TypeEnum.None,
        },
        VirtualMachineAdapter = new QuestAdapter
        {
            Version = 6,
            ObjectFormat = 2,
            ExtraBindDataVersion = 3,
        },
    };

    var script = new ScriptEntry
    {
        Name = "DMUITestQuest",
        Flags = ScriptEntry.Flag.Local,
    };
    script.Properties.Add(new ScriptBoolProperty
    {
        Name = "Enabled",
        Flags = ScriptProperty.Flag.Edited,
        Data = true,
    });
    script.Properties.Add(new ScriptIntProperty
    {
        Name = "IntegerValue",
        Flags = ScriptProperty.Flag.Edited,
        Data = 3,
    });
    script.Properties.Add(new ScriptFloatProperty
    {
        Name = "FloatValue",
        Flags = ScriptProperty.Flag.Edited,
        Data = 0.5f,
    });
    script.Properties.Add(new ScriptStringProperty
    {
        Name = "TextValue",
        Flags = ScriptProperty.Flag.Edited,
        Data = "Initial value",
    });
    script.Properties.Add(new ScriptIntProperty
    {
        Name = "ChoiceValue",
        Flags = ScriptProperty.Flag.Edited,
        Data = 0,
    });
    script.Properties.Add(new ScriptIntProperty
    {
        Name = "ActionCount",
        Flags = ScriptProperty.Flag.Edited,
        Data = 0,
    });
    script.Properties.Add(new ScriptStringProperty
    {
        Name = "LastAction",
        Flags = ScriptProperty.Flag.Edited,
        Data = "None",
    });
    quest.VirtualMachineAdapter.Scripts.Add(script);
    mod.Quests.Add(quest);

    mod.Globals.Add(new GlobalFloat(new FormKey(modKey, 0x801), Fallout4Release.Fallout4)
    {
        EditorID = "DMUITestGlobalFloat",
        Data = 0.5f,
    });
    mod.Globals.Add(new GlobalInt(new FormKey(modKey, 0x802), Fallout4Release.Fallout4)
    {
        EditorID = "DMUITestGlobalInt",
        Data = 3,
    });
    mod.Globals.Add(new GlobalFloat(new FormKey(modKey, 0x803), Fallout4Release.Fallout4)
    {
        EditorID = "DMUITestGlobalBool",
        Data = 1.0f,
    });
    mod.Globals.Add(new GlobalInt(new FormKey(modKey, 0x804), Fallout4Release.Fallout4)
    {
        EditorID = "DMUITestGlobalChoice",
        Data = 0,
    });

    mod.BeginWrite
        .ToPath(path)
        .WithNoLoadOrder()
        .NoMastersListContentCheck()
        .Write();
}

static void NormalizeCompiledPex(string inputPath, string outputPath, string expectedSourceFileName)
{
    Require(File.Exists(inputPath), $"Caprica did not produce {inputPath}");
    var compiled = ReadPex(inputPath);
    Require(
        Path.GetFileName(compiled.Header.SourceFileName)
            .Equals(expectedSourceFileName, StringComparison.OrdinalIgnoreCase),
        $"{inputPath} reports unexpected source file {compiled.Header.SourceFileName}.");

    var normalized = compiled with
    {
        Header = compiled.Header with
        {
            CompilationTime = 0,
            SourceFileName = expectedSourceFileName,
            UserName = string.Empty,
            ComputerName = string.Empty,
        },
    };
    WritePex(outputPath, normalized);

    var reopened = ReadPex(outputPath);
    Require(
        reopened.Header == normalized.Header
        && reopened.Remainder.AsSpan().SequenceEqual(normalized.Remainder),
        $"Normalized PEX did not round-trip exactly: {outputPath}");
}

static void ValidatePlugin(string path, string fixtureDataDirectory)
{
    var modKey = ModKey.FromNameAndExtension(PluginName);
    var mod = Fallout4Mod.CreateFromBinaryOverlay(
        new ModPath(modKey, path),
        Fallout4Release.Fallout4);

    Require(mod.IsSmallMaster, "Plugin is not flagged ESL/light.");
    Require(
        mod.ModHeader.MasterReferences.Select(master => master.Master).SequenceEqual(
            [ModKey.FromNameAndExtension("Fallout4.esm")]),
        "Plugin masters do not contain exactly Fallout4.esm.");

    var quest = mod.Quests.Single();
    Require(quest.FormKey.ID == QuestFormId, "Quest FormID is not local 0x800.");
    Require(quest.EditorID == "DMUITestQuest", "Quest EditorID mismatch.");
    var questData = quest.Data
        ?? throw new InvalidDataException("Quest data is missing.");
    Require(
        questData.Flags == (Quest.Flag.StartGameEnabled | Quest.Flag.RunOnce),
        "Quest startup flags mismatch.");
    Require(questData.Type == Quest.TypeEnum.None, "Quest type mismatch.");
    Require(
        (quest.Aliases?.Count ?? 0) == 0
        && quest.Objectives.Count == 0
        && quest.Stages.Count == 0,
        "Quest unexpectedly contains gameplay aliases, objectives, or stages.");
    var adapter = quest.VirtualMachineAdapter
        ?? throw new InvalidDataException("Quest VMAD is missing.");
    Require(
        adapter.Version == 6 && adapter.ObjectFormat == 2,
        "Quest VMAD is not Fallout 4 version 6/object format 2.");
    Require(adapter.ExtraBindDataVersion == 3, "Quest VMAD bind-data version mismatch.");
    Require(
        string.IsNullOrEmpty(adapter.Script.Name)
        && adapter.Script.Properties.Count == 0
        && adapter.Fragments.Count == 0
        && adapter.Aliases.Count == 0,
        "Quest unexpectedly contains fragments or fragment aliases.");

    var script = adapter.Scripts.Single();
    Require(script.Name == "DMUITestQuest", "Quest VMAD script mismatch.");
    Require(script.Properties.Count == 7, "Quest VMAD property count mismatch.");
    RequireProperty<ScriptBoolProperty>(script, "Enabled", property => property.Data);
    RequireProperty<ScriptIntProperty>(script, "IntegerValue", property => property.Data == 3);
    RequireProperty<ScriptFloatProperty>(script, "FloatValue", property => property.Data == 0.5f);
    RequireProperty<ScriptStringProperty>(script, "TextValue", property => property.Data == "Initial value");
    RequireProperty<ScriptIntProperty>(script, "ChoiceValue", property => property.Data == 0);
    RequireProperty<ScriptIntProperty>(script, "ActionCount", property => property.Data == 0);
    RequireProperty<ScriptStringProperty>(script, "LastAction", property => property.Data == "None");

    var globals = mod.Globals.ToDictionary(global => global.FormKey.ID);
    Require(globals.Count == 4, "Global record count mismatch.");
    RequireGlobal<IGlobalFloatGetter>(globals, 0x801, "DMUITestGlobalFloat", global => global.Data == 0.5f);
    RequireGlobal<IGlobalIntGetter>(globals, 0x802, "DMUITestGlobalInt", global => global.Data == 3);
    RequireGlobal<IGlobalFloatGetter>(globals, 0x803, "DMUITestGlobalBool", global => global.Data == 1.0f);
    RequireGlobal<IGlobalIntGetter>(globals, 0x804, "DMUITestGlobalChoice", global => global.Data == 0);

    ValidateCanonicalConfig(fixtureDataDirectory, quest, script, globals);
}

static void ValidateCanonicalConfig(
    string fixtureDataDirectory,
    IQuestGetter quest,
    IScriptEntryGetter script,
    IReadOnlyDictionary<uint, IGlobalGetter> globals)
{
    var configPath = Path.Combine(
        fixtureDataDirectory,
        "MCM",
        "Config",
        "DMUITests",
        "config.json");
    Require(File.Exists(configPath), $"Canonical config is missing: {configPath}");

    using var document = JsonDocument.Parse(File.ReadAllText(configPath));
    var root = document.RootElement;
    Require(root.GetProperty("modName").GetString() == "DMUITests", "Canonical config modName mismatch.");
    Require(
        root.GetProperty("pluginRequirements")
            .EnumerateArray()
            .Select(requirement => requirement.GetString())
            .SequenceEqual([PluginName]),
        "Canonical config plugin requirements do not contain exactly DMUITests.esp.");

    foreach (var page in root.GetProperty("pages").EnumerateArray())
    {
        foreach (var item in page.GetProperty("content").EnumerateArray())
        {
            if (item.TryGetProperty("valueOptions", out var valueOptions)
                && valueOptions.TryGetProperty("sourceType", out var sourceTypeElement))
            {
                ValidateCanonicalValueReference(
                    sourceTypeElement.GetString() ?? string.Empty,
                    valueOptions,
                    quest,
                    script,
                    globals);
            }

            if (item.TryGetProperty("action", out var action)
                && action.GetProperty("type").GetString() == "CallFunction")
            {
                var formId = ParseFixtureForm(action.GetProperty("form").GetString());
                Require(formId == quest.FormKey.ID, "Canonical CallFunction action does not target quest 0x800.");
                if (action.TryGetProperty("scriptName", out var scriptName))
                {
                    Require(
                        scriptName.GetString() == script.Name,
                        "Canonical CallFunction action script does not match the attached VMAD script.");
                }
            }
        }
    }
}

static void ValidateCanonicalValueReference(
    string sourceType,
    JsonElement valueOptions,
    IQuestGetter quest,
    IScriptEntryGetter script,
    IReadOnlyDictionary<uint, IGlobalGetter> globals)
{
    if (sourceType.StartsWith("GlobalValue", StringComparison.Ordinal))
    {
        var formId = ParseFixtureForm(valueOptions.GetProperty("sourceForm").GetString());
        Require(globals.TryGetValue(formId, out var global), $"Canonical config references missing global 0x{formId:X3}.");
        var defaultValue = valueOptions.GetProperty("default");
        var matches = sourceType switch
        {
            "GlobalValueFloat" => global is IGlobalFloatGetter typed
                && typed.Data == defaultValue.GetSingle(),
            "GlobalValueInt" => global is IGlobalIntGetter typed
                && typed.Data == defaultValue.GetInt32(),
            "GlobalValueBool" => global is IGlobalFloatGetter typed
                && (typed.Data != 0.0f) == defaultValue.GetBoolean(),
            _ => throw new InvalidDataException($"Unsupported canonical global source type: {sourceType}"),
        };
        Require(matches, $"Canonical {sourceType} reference 0x{formId:X3} type or default mismatch.");
        return;
    }

    if (!sourceType.StartsWith("PropertyValue", StringComparison.Ordinal))
    {
        return;
    }

    var propertyFormId = ParseFixtureForm(valueOptions.GetProperty("sourceForm").GetString());
    Require(propertyFormId == quest.FormKey.ID, "Canonical property reference does not target quest 0x800.");
    Require(
        valueOptions.GetProperty("scriptName").GetString() == script.Name,
        "Canonical property reference script does not match the attached VMAD script.");

    var propertyName = valueOptions.GetProperty("propertyName").GetString() ?? string.Empty;
    var property = script.Properties.SingleOrDefault(property => property.Name == propertyName)
        ?? throw new InvalidDataException($"Canonical config references missing VMAD property {propertyName}.");
    var propertyDefault = valueOptions.GetProperty("default");
    var propertyMatches = sourceType switch
    {
        "PropertyValueBool" => property is IScriptBoolPropertyGetter typed
            && typed.Data == propertyDefault.GetBoolean(),
        "PropertyValueInt" => property is IScriptIntPropertyGetter typed
            && typed.Data == propertyDefault.GetInt32(),
        "PropertyValueFloat" => property is IScriptFloatPropertyGetter typed
            && typed.Data == propertyDefault.GetSingle(),
        "PropertyValueString" => property is IScriptStringPropertyGetter typed
            && typed.Data == propertyDefault.GetString(),
        _ => throw new InvalidDataException($"Unsupported canonical property source type: {sourceType}"),
    };
    Require(propertyMatches, $"Canonical {sourceType} property {propertyName} type or default mismatch.");
}

static uint ParseFixtureForm(string? sourceForm)
{
    var reference = string.IsNullOrWhiteSpace(sourceForm)
        ? throw new InvalidDataException("Canonical fixture form reference is empty.")
        : sourceForm;
    var parts = reference.Split('|', 2);
    Require(
        parts.Length == 2 && parts[0].Equals(PluginName, StringComparison.OrdinalIgnoreCase),
        $"Canonical fixture form reference has the wrong plugin: {sourceForm}");
    Require(
        uint.TryParse(parts[1], NumberStyles.HexNumber, CultureInfo.InvariantCulture, out var formId),
        $"Canonical fixture form reference has an invalid FormID: {sourceForm}");
    return formId;
}

static Fallout4Pex ReadPex(string path)
{
    using var stream = File.OpenRead(path);
    using var reader = new BinaryReader(stream, Encoding.UTF8, leaveOpen: false);
    var magic = reader.ReadUInt32();
    var majorVersion = reader.ReadByte();
    var minorVersion = reader.ReadByte();
    var gameId = reader.ReadUInt16();
    var compilationTime = reader.ReadInt64();
    var sourceFileName = ReadPexString(reader);
    var userName = ReadPexString(reader);
    var computerName = ReadPexString(reader);
    var remainderLength = checked((int)(stream.Length - stream.Position));
    var remainder = reader.ReadBytes(remainderLength);

    Require(magic == 0xFA57C0DE, $"{path} has an invalid PEX magic number.");
    Require(
        majorVersion == 3 && minorVersion == 9 && gameId == 2,
        $"{path} is not a Fallout 4 PEX 3.9 file.");
    Require(remainder.Length == remainderLength, $"{path} ended while reading the PEX remainder.");

    return new Fallout4Pex(
        new Fallout4PexHeader(
            magic,
            majorVersion,
            minorVersion,
            gameId,
            compilationTime,
            sourceFileName,
            userName,
            computerName),
        remainder);
}

static void WritePex(string path, Fallout4Pex pex)
{
    var temporaryPath = path + ".tmp";
    try
    {
        using (var stream = File.Create(temporaryPath))
        using (var writer = new BinaryWriter(stream, Encoding.UTF8, leaveOpen: false))
        {
            writer.Write(pex.Header.Magic);
            writer.Write(pex.Header.MajorVersion);
            writer.Write(pex.Header.MinorVersion);
            writer.Write(pex.Header.GameId);
            writer.Write(pex.Header.CompilationTime);
            WritePexString(writer, pex.Header.SourceFileName);
            WritePexString(writer, pex.Header.UserName);
            WritePexString(writer, pex.Header.ComputerName);
            writer.Write(pex.Remainder);
        }
        File.Move(temporaryPath, path, overwrite: true);
    }
    finally
    {
        File.Delete(temporaryPath);
    }
}

static string ReadPexString(BinaryReader reader)
{
    var length = reader.ReadUInt16();
    var bytes = reader.ReadBytes(length);
    Require(bytes.Length == length, "PEX ended while reading a header string.");
    return Encoding.UTF8.GetString(bytes);
}

static void WritePexString(BinaryWriter writer, string value)
{
    var bytes = Encoding.UTF8.GetBytes(value);
    Require(bytes.Length <= ushort.MaxValue, "PEX header string is too long.");
    writer.Write((ushort)bytes.Length);
    writer.Write(bytes);
}

static void RequireProperty<TProperty>(
    IScriptEntryGetter script,
    string name,
    Func<TProperty, bool> predicate)
    where TProperty : class, IScriptPropertyGetter
{
    var property = script.Properties.SingleOrDefault(property => property.Name == name);
    var typedProperty = property as TProperty
        ?? throw new InvalidDataException($"VMAD property {name} has the wrong type.");
    Require(predicate(typedProperty), $"VMAD property {name} mismatch.");
    Require(typedProperty.Flags == ScriptProperty.Flag.Edited, $"VMAD property {name} is not marked edited.");
}

static void RequireGlobal<TGlobal>(
    IReadOnlyDictionary<uint, IGlobalGetter> globals,
    uint formId,
    string editorId,
    Func<TGlobal, bool> valuePredicate)
    where TGlobal : class, IGlobalGetter
{
    Require(globals.TryGetValue(formId, out var global), $"Missing global 0x{formId:X3}.");
    var typedGlobal = global as TGlobal
        ?? throw new InvalidDataException($"Global 0x{formId:X3} has the wrong FNAM type.");
    Require(typedGlobal.EditorID == editorId, $"Global 0x{formId:X3} EditorID mismatch.");
    Require(valuePredicate(typedGlobal), $"Global 0x{formId:X3} value mismatch.");
}

static void Require(bool condition, string message)
{
    if (!condition)
    {
        throw new InvalidDataException(message);
    }
}

sealed record Fallout4PexHeader(
    uint Magic,
    byte MajorVersion,
    byte MinorVersion,
    ushort GameId,
    long CompilationTime,
    string SourceFileName,
    string UserName,
    string ComputerName);

sealed record Fallout4Pex(Fallout4PexHeader Header, byte[] Remainder);
