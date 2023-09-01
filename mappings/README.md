#  Customizing Address Mapping 

You can specify which physical address is mapped to which channel/rank/bank/row/column.

**Please note that this feature is verified for DDR3 only.**  Thus, `Memory.h:124` checks the specs. You can relax the condition for others if you promise to be careful.

## Enabling custom mapping

```bash
./ramulator <config_file> --mode=cpu/dram --stats <stat_file> --mapping <mapping_file> trace0 trace1 ...
```
> --mapping option is completely optional. If not specified, Ramulator uses the default mapping, which is RoBaRaCoCh defined in `Memory.h:81`.

## Syntax of mapping file

### Commenting like `bash`
Just put `#` and the rest in that line is ignored. 

### Bit Indices
Please note that index of the least significant bit is 0.

Please note that at this stage of the simulator, physical addresses do not include cacheline offset bits.

### Bit assignments
```
Ba 2 = 13 # BankAddress[2] := PhysicalAddress[13] 
```

### Array assignments
```
Ba  2:0 = 5:3 # BankAddress[2:0] := PhysicalAddress[5:3]
``` 

### Randomization
Zhang et al. proposed an XOR based bank randomization to reduce the row buffer conflicts. [1]  To enable it in Ramulator, you can specify which bits to xor and assign to where.  
```
Ba 0 = 0 13 # BankAddress[0] := PhysicalAddress[0] xor PhysicalAddress[13]
Ba 1 = 1 7 15 # BankAddress[0] := PhysicalAddress[1] xor PhysicalAddress[7] xor PhysicalAddress[15]
```

### Keywords

Please use the following bigrams of each level:
 - Channel: Ch
 - Rank: Ra
 - Bank Group: Bg
 - Bank: Ba
 - Subarray: Sa
 - Row: Ro
 - Column: Co  

## Examples
Please refer to the individual files. 

[1] Zhao Zhang, Zhichun Zhu, Xiaodong Zhang: [A permutation-based page interleaving scheme to reduce row-buffer conflicts and exploit data locality.](https://ieeexplore.ieee.org/document/898056/) MICRO 2000: 32-41
