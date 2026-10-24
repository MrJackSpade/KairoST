package com.mrjackspade.kairost

import com.mrjackspade.kairo.frontend.CatalogSetting
import com.mrjackspade.kairo.frontend.ChoiceOption
import com.mrjackspade.kairo.frontend.LayeredGameCatalog
import com.mrjackspade.kairo.frontend.MachineOption
import org.json.JSONArray

/**
 * KairoST's catalog records beyond the common fields: the machine a game starts on (`machine`,
 * by Hatari's name for it), its memory, monitor, region and system software, and the second disk it
 * starts with in drive B (media role `floppyB`).
 */
object StCatalog : LayeredGameCatalog.Schema {
    override val platform = "atarist"
    override val updateUrl = "https://raw.githubusercontent.com/MrJackSpade/KairoST/main/catalog/core-v3.json"
    override val metadataUrl = "https://raw.githubusercontent.com/MrJackSpade/KairoST/main/catalog/core-v3.meta.json"
    override val coreVersion = 3
    override val selfHostedArt = "/MrJackSpade/KairoST/main/catalog/artwork/art/catalog/"
    override val contentId = Regex("sha256-(?:fd|prg)-v1:[0-9a-f]{64}")
    override val mediaExtensions = Regex("\\.(?:zip|7z|st|msa|stx|dim|prg|tos|ttp|app|gtp)$", RegexOption.IGNORE_CASE)

    /** The model, by Hatari's name for it. Each later model runs most of the earlier ones' games. */
    val machine = MachineOption("machine", "Machine", listOf(
        ChoiceOption("st", "Atari ST"),
        ChoiceOption("megast", "Atari Mega ST"),
        ChoiceOption("ste", "Atari STE"),
        ChoiceOption("megaste", "Atari Mega STE"),
        ChoiceOption("tt", "Atari TT"),
        ChoiceOption("falcon", "Atari Falcon")), "st")

    /** ST RAM in KiB. */
    val memory = MachineOption("memory", "Memory", listOf(
        ChoiceOption("512", "512 KB"),
        ChoiceOption("1024", "1 MB"),
        ChoiceOption("2048", "2 MB"),
        ChoiceOption("4096", "4 MB")), "1024", numeric = true)

    /** The monitor, by Hatari's name for it: color games need RGB, high resolution ones mono. */
    val monitor = MachineOption("monitor", "Monitor", listOf(
        ChoiceOption("rgb", "Color"),
        ChoiceOption("mono", "Monochrome")), "rgb")

    /**
     * EmuTOS's country (Hatari's code for it): Europe is the UK, at 50 Hz like most ST games
     * expect; USA runs at 60 Hz. A player's own TOS keeps its own.
     */
    val region = MachineOption("region", "Region", listOf(
        ChoiceOption("uk", "Europe (50 Hz)"),
        ChoiceOption("us", "USA (60 Hz)")), "uk")

    const val EMUTOS = "emutos"
    const val OWN_TOS = "tos"

    /** The choices every game has: EmuTOS, which comes with the app, or the player's TOS for its model (StTos). */
    val fixedSystems = listOf(
        ChoiceOption(EMUTOS, "EmuTOS"),
        ChoiceOption(OWN_TOS, "Your TOS for its model"))

    /**
     * The system software a game starts: [fixedSystems], or one TOS image of the player's by name
     * (TosLibrary, "tos:<hash>"), which [images] lists; [actions] add more.
     */
    fun system(images: () -> List<ChoiceOption> = { emptyList() },
               actions: (select: (String) -> Unit) -> List<com.mrjackspade.kairo.frontend.SettingsRow> = { emptyList() }) =
        MachineOption("tos", "System", { fixedSystems + images() }, EMUTOS,
            accepts = { value -> value == EMUTOS || value == OWN_TOS || TosLibrary.ID.matches(value) }, actions = actions)

    /** The System option as the catalog reads it; MainActivity lists the player's images too. */
    val system = system()

    val skipMemoryCheck = MachineOption.skipMemoryCheck()
    val options = listOf(machine, memory, monitor, region, system, skipMemoryCheck)

    override val settings: List<CatalogSetting> = options.map(CatalogSetting::Choice) + CatalogSetting.Drive("floppyB", "Drive B")
    override val fields = CatalogSetting.fields(settings)

    override fun validMedia(role: String, contentId: String) = role == "floppyB" && contentId.startsWith("sha256-fd-v1:")

    override fun validField(field: String, value: Any): Boolean =
        field in fields && CatalogSetting.validSection(settings, field, value)

    override fun validBindings(bindings: JSONArray) = StKeys.bindings.valid(bindings)
}
