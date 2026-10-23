package com.mrjackspade.kairost

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class StTosTest {
    /** A header as TOS images start: a branch, the version, and EmuTOS's mark at 0x2c. */
    private fun header(branch: Int, version: Int, emutos: Boolean = false) = ByteArray(256).also {
        it[0] = (branch shr 8).toByte(); it[1] = branch.toByte()
        it[2] = (version shr 8).toByte(); it[3] = version.toByte()
        if (emutos) "ETOS".toByteArray().copyInto(it, 0x2c)
    }

    // The branches and versions of real images: TOS 1.00 starts 601e, later ones 602e.
    @Test fun findsEachModel() {
        assertEquals(StTos.Model.ST, StTos.model(header(0x601e, 0x0100)))
        assertEquals(StTos.Model.ST, StTos.model(header(0x602e, 0x0104)))
        assertEquals(StTos.Model.STE, StTos.model(header(0x602e, 0x0106)))
        assertEquals(StTos.Model.STE, StTos.model(header(0x602e, 0x0162)))
        assertEquals(StTos.Model.STE, StTos.model(header(0x602e, 0x0206)))
        assertEquals(StTos.Model.TT, StTos.model(header(0x602e, 0x0306)))
        assertEquals(StTos.Model.FALCON, StTos.model(header(0x602e, 0x0404)))
    }

    @Test fun skipsEmuTosAndOtherFiles() {
        assertNull(StTos.model(header(0x602e, 0x0206, emutos = true)))
        assertNull(StTos.model(header(0x602e, 0x0104, emutos = true)))
        // Hatari's cartridge image.
        assertNull(StTos.model(header(0xfa52, 0x235f)))
        assertNull(StTos.model(ByteArray(8)))
    }

    /** The country byte as TOS writes it: country shifted left, with the PAL bit. */
    private fun withCountry(version: Int, country: Int) = header(0x602e, version).also { it[0x1d] = (country shl 1 or 1).toByte() }

    @Test fun ranksEnglishThenLatest() {
        val uk104 = StTos.rank(withCountry(0x0104, 3), StTos.Model.ST)!!
        val us102 = StTos.rank(withCountry(0x0102, 0), StTos.Model.ST)!!
        val de104 = StTos.rank(withCountry(0x0104, 1), StTos.Model.ST)!!
        val uk100 = StTos.rank(withCountry(0x0100, 3), StTos.Model.ST)!!
        assert(uk104 > us102 && us102 > uk100 && uk100 > de104)
        assertNull(StTos.rank(withCountry(0x0206, 3), StTos.Model.ST))
    }

    @Test fun machinesTakeTheirModel() {
        assertEquals(StTos.Model.ST, StTos.forMachine("megast"))
        assertEquals(StTos.Model.STE, StTos.forMachine("megaste"))
        assertEquals(StTos.Model.FALCON, StTos.forMachine("falcon"))
    }
}
