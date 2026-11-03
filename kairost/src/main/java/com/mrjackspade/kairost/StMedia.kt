package com.mrjackspade.kairost

import com.mrjackspade.kairo.frontend.DiskImageLibrary
import com.mrjackspade.kairo.frontend.R as FrontendR
import com.mrjackspade.kairo.frontend.UiText

/** The library's media format, as the app's launch builds it (Kairo#125). */
object StMedia {
    /**
     * Floppy disk images in the formats Hatari reads, hard disk images (st_machine.h Config.hard_disk),
     * and programs, which run from their folder as drive C (Config.program).
     */
    val format = DiskImageLibrary.Format(
        kinds = listOf("st", "msa", "stx", "dim").associateWith { "fd" } +
            listOf("img", "hd").associateWith { "hd" } +
            listOf("prg", "tos", "ttp", "app", "gtp").associateWith { "prg" },
        labels = mapOf("fd" to UiText.res(FrontendR.string.kairo_floppy_disk),
            "hd" to UiText.res(FrontendR.string.kairo_hard_disk), "prg" to UiText.res(FrontendR.string.kairo_program)),
        maxBytes = mapOf("fd" to 16L * 1024 * 1024, "hd" to 4L * 1024 * 1024 * 1024, "prg" to 16L * 1024 * 1024),
        guestName = "Atari ST",
        programs = setOf("prg"))
}
