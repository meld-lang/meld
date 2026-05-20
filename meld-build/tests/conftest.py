"""Pytest configuration and shared fixtures for Meld build system tests.

Provides Hypothesis settings profiles, shared test strategies, and
fixtures used across property-based and unit tests.
"""

import os
import sys
from pathlib import Path

import pytest
from hypothesis import settings, HealthCheck, Phase

# ---------------------------------------------------------------------------
# Hypothesis settings profiles
# ---------------------------------------------------------------------------

# Default profile: 100 examples minimum as required by the design document
settings.register_profile(
    "default",
    max_examples=100,
    suppress_health_check=[HealthCheck.too_slow],
    phases=[Phase.explicit, Phase.reuse, Phase.generate, Phase.shrink],
)

# CI profile: more examples for thorough validation
settings.register_profile(
    "ci",
    max_examples=500,
    suppress_health_check=[HealthCheck.too_slow],
)

# Dev profile: fewer examples for fast iteration
settings.register_profile(
    "dev",
    max_examples=20,
    suppress_health_check=[HealthCheck.too_slow],
)

settings.load_profile(os.getenv("HYPOTHESIS_PROFILE", "default"))

# ---------------------------------------------------------------------------
# Path helpers
# ---------------------------------------------------------------------------

# Root of the meld-build package
MELD_BUILD_ROOT = Path(__file__).resolve().parent.parent
# Root of the rules directory
RULES_ROOT = MELD_BUILD_ROOT / "rules" / "meld"
# Workspace root (two levels up from meld-build)
WORKSPACE_ROOT = MELD_BUILD_ROOT.parent


# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------

@pytest.fixture
def rules_root():
    """Return the path to the Meld Bazel rules directory."""
    return RULES_ROOT


@pytest.fixture
def meld_build_root():
    """Return the path to the meld-build package root."""
    return MELD_BUILD_ROOT


@pytest.fixture
def workspace_root():
    """Return the path to the workspace root."""
    return WORKSPACE_ROOT
