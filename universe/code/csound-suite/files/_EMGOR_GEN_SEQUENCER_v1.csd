<Cabbage>


form caption("Untitled") size(400, 300), guiMode("queue"), pluginId("def1")
button bounds(0, 90, 325, 187) channel("backgrn1") outlineThickness(5), corners(0), outlineColour(23, 19, 120, 255) text("", "") active(0) 


</Cabbage>
<CsoundSynthesizer>
<CsOptions>
-n -d -+rtmidi=NULL -M0 -m0d --midi-key-cps=4 --midi-velocity-amp=5
</CsOptions>
<CsInstruments>
; Initialize the global variables. 
ksmps = 32
nchnls = 2
0dbfs = 1


instr grid

    iGridChan = p4
    iGridX = p5
    iGridY = p6
    
    SGridCode sprintf "bounds(%d, %d, 15, 15) channel(\"%d\")  colour:0(24, 23, 58, 255) colour:1(23, 19, 195, 255)", iGridX, iGridY, iGridChan
    cabbageCreate "checkbox", SGridCode
    
endin


;instr SEQUENCER
;
;    kCnt init 0
;    kNoteIndex init 0
;    while kCnt < 8 do
;        gkNotes[kCnt] = 60+kCnt
;        kCnt+=1
;    od
;
;    if metro(1) == 1 then
;        event "i", 1, 0, .5, gkNotes[kNoteIndex]
;        kNoteIndex = kNoteIndex<6 ? kNoteIndex+1 : 0
;    endif
;    
;endin


</CsInstruments>
<CsScore>
;causes Csound to run for about 7000 years...
f0 z
i"grid" 0 z 1 4 96 
i"grid" 0 z 2 4 116 
i"grid" 0 z 3 4 136 
i"grid" 0 z 4 4 156 
i"grid" 0 z 5 4 176 
i"grid" 0 z 6 4 196 
i"grid" 0 z 7 4 216 
i"grid" 0 z 8 4 236 
i"grid" 0 z 9 4 256 

i"grid" 0 z 11 24 96 
i"grid" 0 z 12 24 116 
i"grid" 0 z 13 24 136 
i"grid" 0 z 14 24 156 
i"grid" 0 z 15 24 176 
i"grid" 0 z 16 24 196 
i"grid" 0 z 17 24 216 
i"grid" 0 z 18 24 236 
i"grid" 0 z 19 24 256 

i"grid" 0 z 21 44 96 
i"grid" 0 z 22 44 116 
i"grid" 0 z 23 44 136 
i"grid" 0 z 24 44 156 
i"grid" 0 z 25 44 176 
i"grid" 0 z 26 44 196 
i"grid" 0 z 27 44 216 
i"grid" 0 z 28 44 236 
i"grid" 0 z 29 44 256 

i"grid" 0 z 31 64 96 
i"grid" 0 z 32 64 116 
i"grid" 0 z 33 64 136 
i"grid" 0 z 34 64 156 
i"grid" 0 z 35 64 176 
i"grid" 0 z 36 64 196 
i"grid" 0 z 37 64 216 
i"grid" 0 z 38 64 236 
i"grid" 0 z 39 64 256 

i"grid" 0 z 41 84 96 
i"grid" 0 z 42 84 116 
i"grid" 0 z 43 84 136 
i"grid" 0 z 44 84 156 
i"grid" 0 z 45 84 176 
i"grid" 0 z 46 84 196 
i"grid" 0 z 47 84 216 
i"grid" 0 z 48 84 236 
i"grid" 0 z 49 84 256 

i"grid" 0 z 51 104 96 
i"grid" 0 z 52 104 116 
i"grid" 0 z 53 104 136 
i"grid" 0 z 54 104 156 
i"grid" 0 z 55 104 176 
i"grid" 0 z 56 104 196 
i"grid" 0 z 57 104 216 
i"grid" 0 z 58 104 236 
i"grid" 0 z 59 104 256 

i"grid" 0 z 61 124 96 
i"grid" 0 z 62 124 116 
i"grid" 0 z 63 124 136 
i"grid" 0 z 64 124 156 
i"grid" 0 z 65 124 176 
i"grid" 0 z 66 124 196 
i"grid" 0 z 67 124 216 
i"grid" 0 z 68 124 236 
i"grid" 0 z 69 124 256 

i"grid" 0 z 71 144 96 
i"grid" 0 z 72 144 116 
i"grid" 0 z 73 144 136 
i"grid" 0 z 74 144 156 
i"grid" 0 z 75 144 176 
i"grid" 0 z 76 144 196 
i"grid" 0 z 77 144 216 
i"grid" 0 z 78 144 236 
i"grid" 0 z 79 144 256 

i"grid" 0 z 81 164 96 
i"grid" 0 z 82 164 116 
i"grid" 0 z 83 164 136 
i"grid" 0 z 84 164 156 
i"grid" 0 z 85 164 176 
i"grid" 0 z 86 164 196 
i"grid" 0 z 87 164 216 
i"grid" 0 z 88 164 236 
i"grid" 0 z 89 164 256 

i"grid" 0 z 91 184 96 
i"grid" 0 z 92 184 116 
i"grid" 0 z 93 184 136 
i"grid" 0 z 94 184 156 
i"grid" 0 z 95 184 176 
i"grid" 0 z 96 184 196 
i"grid" 0 z 97 184 216 
i"grid" 0 z 98 184 236 
i"grid" 0 z 99 184 256 

i"grid" 0 z 101 204 96 
i"grid" 0 z 102 204 116 
i"grid" 0 z 103 204 136 
i"grid" 0 z 104 204 156 
i"grid" 0 z 105 204 176 
i"grid" 0 z 106 204 196 
i"grid" 0 z 107 204 216 
i"grid" 0 z 108 204 236 
i"grid" 0 z 109 204 256 

i"grid" 0 z 110 224 96 
i"grid" 0 z 112 224 116 
i"grid" 0 z 113 224 136 
i"grid" 0 z 114 224 156 
i"grid" 0 z 115 224 176 
i"grid" 0 z 116 224 196 
i"grid" 0 z 117 224 216 
i"grid" 0 z 118 224 236 
i"grid" 0 z 119 224 256 

i"grid" 0 z 121 244 96 
i"grid" 0 z 122 244 116 
i"grid" 0 z 123 244 136 
i"grid" 0 z 124 244 156 
i"grid" 0 z 125 244 176 
i"grid" 0 z 126 244 196 
i"grid" 0 z 127 244 216 
i"grid" 0 z 128 244 236 
i"grid" 0 z 129 244 256 

i"grid" 0 z 131 264 96 
i"grid" 0 z 132 264 116 
i"grid" 0 z 133 264 136 
i"grid" 0 z 134 264 156 
i"grid" 0 z 135 264 176 
i"grid" 0 z 136 264 196 
i"grid" 0 z 137 264 216 
i"grid" 0 z 138 264 236 
i"grid" 0 z 139 264 256 

i"grid" 0 z 141 284 96 
i"grid" 0 z 142 284 116 
i"grid" 0 z 143 284 136 
i"grid" 0 z 144 284 156 
i"grid" 0 z 145 284 176 
i"grid" 0 z 146 284 196 
i"grid" 0 z 147 284 216 
i"grid" 0 z 148 284 236 
i"grid" 0 z 149 284 256 

i"grid" 0 z 151 304 96 
i"grid" 0 z 152 304 116 
i"grid" 0 z 153 304 136 
i"grid" 0 z 154 304 156 
i"grid" 0 z 155 304 176 
i"grid" 0 z 156 304 196 
i"grid" 0 z 157 304 216 
i"grid" 0 z 158 304 236 
i"grid" 0 z 159 304 256 
</CsScore>
</CsoundSynthesizer>
