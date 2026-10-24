"""Simple two-point linear calibration helper for the wetness ADC."""
import argparse

def gain_offset(dry: float, wet: float, dry_ref: float, wet_ref: float):
    if dry == wet:
        raise ValueError('calibration readings must differ')
    gain=(wet_ref-dry_ref)/(wet-dry)
    return gain, dry_ref-gain*dry

def main():
    p=argparse.ArgumentParser(); p.add_argument('--dry',type=float,required=True); p.add_argument('--wet',type=float,required=True)
    a=p.parse_args(); print('gain,offset =',gain_offset(a.dry,a.wet,0.0,1000.0))
if __name__=='__main__': main()
