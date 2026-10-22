package com.mrjackspade.kairost

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
    override val contentId = Regex("sha256-fd-v1:[0-9a-f]{64}")
    override val mediaExtensions = Regex("\\.(?:zip|7z|st|msa|stx|dim)$", RegexOption.IGNORE_CASE)

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

    /** EmuTOS comes with the app; the player's own TOS image is the firmware file tos.img. */
    val system = MachineOption("tos", "System", listOf(
        ChoiceOption(EMUTOS, "EmuTOS"),
        ChoiceOption(OWN_TOS, "Your TOS image")), EMUTOS)

    val options = listOf(machine, memory, monitor, region, system)

    override val fields: Map<String, Set<String>?> = mapOf(MachineOption.SECTION to options.map { it.id }.toSet())
    override val singleMediaRoles = setOf("floppyB")

    override fun validMedia(role: String, contentId: String) = role == "floppyB" && contentId.startsWith("sha256-fd-v1:")

    override fun validField(field: String, value: Any): Boolean =
        field == MachineOption.SECTION && MachineOption.validSection(options, value)

    override fun validBindings(bindings: JSONArray) = StKeys.bindings.valid(bindings)
}
