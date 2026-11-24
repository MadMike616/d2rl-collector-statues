"""Generate minimal synthetic test tables; never copy or install game data."""
from pathlib import Path
import argparse
import re

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    excel=args.output/'data/global/excel'
    excel.mkdir(parents=True,exist_ok=True)
    levels=['Id\tAct\tName\tLevelName\tLevelType',
            '2\t0\tTest wilderness\tTestWilderness\t2',
            '39\t0\tAct 1 - Moo Moo Farm\tTestCowLevel\t2']
    types=[10,8,4,3,33,35]
    for floor in range(1,61):
        levels.append(f'{137+floor}\t4\tTest floor {floor}\tGrailerDescentFloor{floor}\t{types[(floor-1)//10]}')
    levels.append('198\t4\tTest entrance\tGrailerDescentEntrance\t10')
    (excel/'levels.txt').write_text('\n'.join(levels)+'\n',encoding='utf-8')
    solo=['griswold','smith','radament','temptress','andariel','duriel','mephisto','diablo',
          'baal','bloodraven','summoner','izual','deseis','infector','vizier','lister']
    council=['ismail','geleb','toorc','bremm','wyand','maffer']
    monsters=['Id\t*hcIdx']
    for floor in range(10,61,10):
        prefix='descent' if floor==10 else f'descent{floor}'
        names=[f'{prefix}_{name}{suffix}' for name in solo for suffix in ['', '_2', '_3']]
        names += [f'{prefix}_{name}' for name in council]
        for name in names:monsters.append(f'{name}\t{3000+len(monsters)-1}')
    (excel/'monstats.txt').write_text('\n'.join(monsters)+'\n',encoding='utf-8')
    catalog=Path(__file__).resolve().parents[1]/'src/statue_definitions.h'
    entries=re.findall(r'^\s*\{\s*(\d+)\s*,\s*D2RL::Items::MakeItemCode\("([^"\n]+)"\)',catalog.read_text(encoding='utf-8'),re.M)
    if not entries:raise RuntimeError('No statue registrations found')
    uniques=['index\tcode\tnolimit']+[f'Test statue {number}\t{code}\t1' for number,code in entries]
    (excel/'uniqueitems.txt').write_text('\n'.join(uniques)+'\n',encoding='utf-8')

if __name__=='__main__':main()
