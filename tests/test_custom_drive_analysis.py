"""Metric sanity checks independent of the C++ build and sonic preferences."""
import importlib.util
from pathlib import Path
import numpy as np

spec = importlib.util.spec_from_file_location('drive_analysis', Path(__file__).resolve().parents[1] / 'examples/benchmark_custom_drive.py')
analysis = importlib.util.module_from_spec(spec)
spec.loader.exec_module(analysis)


def test_offline_matching_and_silence():
    x = np.array([-.2, .2] * 10)
    reference = x * 3
    np.testing.assert_allclose(analysis.level_match(x, reference), reference)
    np.testing.assert_array_equal(analysis.level_match(x * 0, reference), x * 0)
    np.testing.assert_array_equal(x, np.array([-.2, .2] * 10))


def test_alias_proxy_known_coherent_component():
    sr = 48000
    t = np.arange(sr) / sr
    carrier = np.sin(2 * np.pi * 7013 * t)
    y = carrier + .1 * np.sin(2 * np.pi * 5000 * t)
    assert abs(analysis.residual_dbc(y, sr, 7013) + 20) < 1e-8
    assert analysis.residual_dbc(carrier, sr, 7013) < -200


def test_harmonic_metric_known_second_harmonic():
    sr = 48000
    t = np.arange(sr) / sr
    y = np.sin(2 * np.pi * 440 * t) + .1 * np.sin(2 * np.pi * 880 * t)
    assert abs(analysis.metrics(y, sr, 440)['thd'] - .1) < 1e-6


def test_sweep_steps_validation():
    import pytest
    import argparse
    assert analysis.sweep_values('0,12.5,100') == [0,12.5,100]
    for value in ('nan', '-1', '101', '0,0'):
        with pytest.raises(argparse.ArgumentTypeError):
            analysis.sweep_values(value)


def test_listening_render_preserves_raw_stereo_and_common_rms(tmp_path):
    from scipy.io import wavfile
    signal = np.column_stack([np.tile([-.2,.2],32),np.tile([-.1,.1],32)])
    def render(x, params, rate):
        return x * (1 + sum(params.values()) / 100)
    manifest = analysis.write_listening_sweeps(tmp_path,signal,48000,render,[0,50,100])
    assert len(manifest) == 15
    rate, raw = wavfile.read(tmp_path/'raw'/'drive_100.wav')
    _, matched = wavfile.read(tmp_path/'level_matched'/'drive_100.wav')
    assert rate == 48000
    np.testing.assert_allclose(raw,signal*2)
    np.testing.assert_allclose(matched,signal)
    assert all(abs(r['matched_rms']-analysis.calculate_rms(signal))<1e-12 for r in manifest)
    assert (tmp_path/'listening_manifest.json').exists()


def test_cross_macro_renders_have_explicit_parameter_pairs(tmp_path):
    from scipy.io import wavfile
    signal=np.tile([-.1,.1],32)
    calls=[]
    def render(x,params,rate):
        calls.append(dict(params))
        return x*(1+params.get('crunch',0)/100+params.get('grain',0)/200)
    manifest=analysis.write_listening_sweeps(tmp_path,signal,48000,render,[0,100],contrasts=True)
    assert len(manifest)==14
    assert {'crunch':0,'grain':100} in calls
    assert {'crunch':100,'grain':0} in calls
    assert {'drive':70,'headroom':0} in calls
    assert {'drive':70,'headroom':100} in calls
    _,a=wavfile.read(tmp_path/'raw'/'crunch000_grain100.wav')
    _,b=wavfile.read(tmp_path/'raw'/'crunch100_grain000.wav')
    np.testing.assert_allclose(a,signal*1.5)
    np.testing.assert_allclose(b,signal*2)
    for name in ('crunch000_grain100','crunch100_grain000'):
        _,matched=wavfile.read(tmp_path/'level_matched'/f'{name}.wav')
        np.testing.assert_allclose(matched,signal)


def test_even_odd_metric_excludes_fundamental():
    sr=48000
    t=np.arange(sr)/sr
    y=np.sin(2*np.pi*440*t)+.1*np.sin(2*np.pi*880*t)+.05*np.sin(2*np.pi*1320*t)
    result=analysis.metrics(y,sr,440)
    assert abs(result['even_odd_db']-10*np.log10(4))<1e-5
